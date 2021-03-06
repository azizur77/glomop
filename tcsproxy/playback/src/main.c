/*
 *     Author: Steve Gribble
 *       Date: Nov. 19th, 1996
 *       File: main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config_tr.h"
#ifdef HAVE_PTHREAD_H
#include "pthread.h"
#endif

#include "bp_timer.h"
#include "http_task.h"
#include "logparse.h"
#include "utils.h"

void timerproc(int id, void *data);
void timeraddproc(int id, void *data);
void speed_proc(int id, void *data);

int numthreads = 0;
extern int numconnecting;
pthread_mutex_t numt_mutex;

int   port, numpersec = 1, ceiling = 400;
double numpersecd;
char *ip_addr;
FILE *outfile = stdout;
char *lport;
struct timeval last;

int main(int argc, char **argv)
{
  struct timeval tv;

  if ((argc != 6) && (argc != 7)) {
    fprintf(stderr, "usage: bp_engine target_IP target_port numpersec listenport ceiling [of]\n");
    exit(1);
  }
  if (sscanf(*(argv+2), "%d", &port) != 1) {
    fprintf(stderr, "usage: bp_engine target_IP target_port numpersec listenport ceiling [of]\n");
    exit(1);
  }
  if (sscanf(*(argv+3), "%lf", &numpersecd) != 1) {
    fprintf(stderr, "usage: bp_engine target_IP target_port numpersec listenport ceiling [of]\n");
    exit(1);
  }
  lport = *(argv+4);
  fprintf(stderr, "Magic port: %s\n", lport);
  ip_addr = *(argv+1);
  if (sscanf(*(argv+5), "%d", &ceiling) != 1) {
    fprintf(stderr, "usage: bp_engine target_IP target_port numpersec listenport ceiling [of]\n");
    exit(1);
  }
  if (argc == 7) {
    outfile = fopen(*(argv+6), "w");
    if (outfile == NULL) {
      fprintf(stderr, "Couldn't open file %s for writing.\n", *(argv+4));
      exit(1);
    }
    setbuf(outfile, NULL);
  } else
    outfile = stdout;

  if (bp_timer_init()) {
    fprintf(stderr, "timer subsystem init failed\n");
    exit(1);
  }
  if (initialize_http_tasks("UC Berkeley Trace Playback Engine (please contact gribble@cs.berkeley.edu for inquiries regarding this access)",
			    "Steven Gribble, http://www.cs.berkeley.edu/~gribble, gribble@cs.berkeley.edu")) {
    fprintf(stderr, "http_tasks subsystem init failed\n");
    exit(1);
  }

  if (pthread_mutex_init(&numt_mutex, NULL) != 0)
    exit(1);

  tv.tv_sec = 1;
  tv.tv_usec = 0;
  bp_timer_add(RELATIVE, tv, speed_proc, (void *) NULL);
  gettimeofday(&last, NULL);
  timeraddproc(1, NULL);

  while(1)
    sleep(500);
  exit(0);
}

void timeraddproc(int id, void *data)
{
  struct timeval  tv, diff;

  while (numthreads > ceiling)
      sleep(1);
  gettimeofday(&tv, NULL);

  if (numpersecd == 1.0) {
    tv.tv_sec += 1;
  } else {
    tv.tv_usec += (int) (((double) 1000000.0) / numpersecd);
    while (tv.tv_usec > 1000000) {
      tv.tv_usec -= 1000000;
      tv.tv_sec += 1;
    }
  }
  {
    lf_entry      *lfntree;

    lfntree = (lf_entry *) malloc(sizeof(lf_entry));
    if (lfntree == NULL) {
      fprintf(stderr, "Out of mem in main.\n");
      exit(1);
    }
    if (lf_get_next_entry(0, lfntree, 3) != 0) {
      fprintf(stderr, "Failed to get next entry.\n");
      exit(1);
    }
    
    bp_timer_add(ABSOLUTE, tv, timerproc, (void *) lfntree);
  }
  bp_timer_add(ABSOLUTE, tv, timeraddproc, (void *) NULL);
  diff = tv_timesub(tv, last);
  if (diff.tv_sec >= 1) {
    fprintf(outfile, "%d threads, %d connecting\n", numthreads, numconnecting);
    last = tv;
  }
  bp_dispatch_signal();
}

void timerproc(int id, void *data)
{
  http_task      t;
  lf_entry      *lfntree = (lf_entry *) data;
  char          *retd;
  int            retl, res;

  if (pthread_mutex_lock(&numt_mutex) < 0)
    exit(1);
  numthreads++;
  if (pthread_mutex_unlock(&numt_mutex) < 0)
    exit(1);
  t.url = (char *) lfntree->url;
  if ( (strcmp(t.url, "-") == 0) ||
       (strlen(t.url) < 5) ||
       (lfntree->sip == 0xFFFFFFFF) ||
       (lfntree->spt == 0xFFFF) ||
       !((strncmp(t.url, "GET ", 4) == 0) ||
	(strncmp(t.url, "POST ", 5) == 0) ||
	(strncmp(t.url, "HEAD ", 5) == 0) ||
	(strncmp(t.url, "PUT " , 4) == 0))
    ) {
    fprintf(stderr, "Ignoring bum url %s\n", t.url);
    free(t.url);
    free(lfntree);
    if (pthread_mutex_lock(&numt_mutex) < 0)
      exit(1);
    numthreads--;
    if (pthread_mutex_unlock(&numt_mutex) < 0)
      exit(1);
    return;
  }
  t.urllen = strlen(t.url);
  t.dst_host = lfntree->sip;
  t.dst_port = lfntree->spt;
  t.client_pragmas = lfntree->cprg;
  t.server_pragmas = lfntree->sprg;
  t.client_if_modified_date = ntohl(lfntree->cims);
  t.server_expires_date = ntohl(lfntree->sexp);
  t.server_last_modified_date = ntohl(lfntree->slmd);
  insert_host_into_url(&t);
  t.dst_host = inet_addr(ip_addr); t.dst_port = htons(port);
  if (t.dst_host == (unsigned long) -1) {
    free(t.url);
    free(lfntree);
    if (pthread_mutex_lock(&numt_mutex) < 0)
      exit(1);
    numthreads--;
    if (pthread_mutex_unlock(&numt_mutex) < 0)
      exit(1);
    return;
  }
  res = do_http_task(t, &retd, &retl);
  if (res == 0) {
    free(retd);
  } else {
    fprintf(outfile, "do_http_task failed. (%d threads, ignoring..)\n", 
	    numthreads);
    lfntree->url = (unsigned char *)t.url;
    lf_dump(outfile, lfntree);
    if (retd)
      free(retd);
  }
  if (t.url)
    free(t.url);
  if (lfntree)
    free(lfntree);
  if (pthread_mutex_lock(&numt_mutex) < 0)
    exit(1);
  numthreads--;
  if (pthread_mutex_unlock(&numt_mutex) < 0)
    exit(1);
}

void speed_proc(int id, void *data)
{
  int speed_fd, conn_fd, done_sock =0;

  conn_fd = slisten(lport);
  if (conn_fd == -1) {
    fprintf(stderr, "slisten failed\n");
    exit(1);
  }
  
  while (1) {
    speed_fd = saccept(conn_fd);
    if (speed_fd < 0) {
      fprintf(stderr, "saccept failed\n");
      exit(1);
    }

    while (!done_sock) {
      char read_buf[1024];
      int  i = 0, startline = 1;

      while (1) {
	if (correct_read(speed_fd, &(read_buf[i]), 1) <= 0) {
	  done_sock = 1;
	  break;
	}
	if (read_buf[i] == '\n') {
	  if (startline == 1)
	    continue;
	  else {
	    read_buf[i] = '\0';
	    if (sscanf(read_buf, "%lf", &numpersecd) != 1) {
	      fprintf(stderr, "bogus data %s, ignoring\n", read_buf);
	    } else {
	      fprintf(stderr, "set rate to %f\n", numpersecd);
	      startline = 1;
	      i = 0;
	      continue;
	    }
	  }
	} else if (startline == 1) {
	    startline = 0;
	    i++;
	  } else
	    i++;
      }
      if (done_sock == 1)
	continue;
    }
    done_sock = 0;
    close(speed_fd);
  }
}
