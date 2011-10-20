import java.io.*;
import java.util.*;

class HTMLTag
{
  public boolean start;
  public StringBuffer name;
  public Vector attributes;
  public Vector values;

  public HTMLTag()
  {
    name = new StringBuffer();
    attributes = new Vector();
    values = new Vector();
  }
}

class HTMLTokenizer 
{
  protected PushbackInputStream in;

  public HTMLTag tag;
  public StringBuffer text;

  public final static int TT_EOF = -1;
  public final static int TT_START_TAG = -2;
  public final static int TT_END_TAG = -3;
  public final static int TT_TEXT = -4;

  public HTMLTokenizer(PushbackInputStream input)
  {
    in = input;
  }

  public int nextToken() throws IOException
  {
    tag = new HTMLTag();
    text = new StringBuffer();

    int c = in.read();
    if (c == -1) {
      return TT_EOF;
    }
    else if (c == '&') {
      c = in.read();
      if (c == -1) {
        text.append('&');
        return TT_TEXT;
      }
      else if (c == '#') {
        c = in.read();
        if (c == -1) {
          text.append("&#");
          return TT_TEXT;
        }
        else if (Character.isDigit((char)c)) {
          text.append(readNCR(c));
          readText();
          return TT_TEXT;
        }
        else {
          text.append("&#");
          text.append((char)c);
          readText();
          return TT_TEXT;
        }
      }
      else if (Character.isLetter((char)c)) {
        text.append(readGER(c));
        readText();
        return TT_TEXT;
      }         
      else {
        text.append('&');
        text.append((char)c);
        readText();
        return TT_TEXT;
      }
    }
    else if (c == '<') {
      c = in.read();
      if (c == -1) {
        text.append('<');
        return TT_TEXT;
      }
      else if (c == '!') {
        c = in.read();
        if (c == -1) {
          text.append("<!");
          return TT_TEXT;
        }
        else if (c == '>') {
          // Empty comment <!> okay
          return nextToken();
        }
        else if (c == '-') {
          c = in.read();
          if (c == -1) {
            text.append("<!-");
            return TT_TEXT;
          }
          else if (c == '-') {
            readCommentDecl();
            return nextToken();
          }
          else {
            text.append("<!-");
            text.append((char)c);
            readText();
            return TT_TEXT;
          }
        }
        else {
          text.append("<!");
          text.append((char)c);
          readText();
          return TT_TEXT;
        }
      }
      else if (c == '/') {
        c = in.read();
        if (c == -1) {
          text.append("</");
          return TT_TEXT;
        }
        else if (Character.isLetter((char)c)) {
          if (readEndTag(c) == 0) {
            return TT_END_TAG;
          }
          else {
            return TT_EOF;
          }
        }
        else {
          text.append("</");
          text.append((char)c);
          readText();
          return TT_TEXT;
        }
      }
      else if (Character.isLetter((char)c)) {
        if (readStartTag(c) == 0) {
          return TT_START_TAG;
        }
        else {
          return TT_EOF;
        }
      }
      else {
        text.append('<');
        text.append((char)c);
        readText();
        return TT_TEXT;
      }
    }
    else {
      text.append((char)c);
      readText();
      return TT_TEXT;
    }
  }

  private void readText() throws IOException
  {
    int c = in.read();
    while (c != -1 && c != '&' && c != '<') {
      text.append((char)c);
      c = in.read();
    }
    if (c != -1) {
      in.unread(c);
    }
  }

  private char readNCR(int c) throws IOException
  {
    StringBuffer ncr = new StringBuffer();
    ncr.append((char)c);

    c = in.read();
    while (c != -1 && Character.isDigit((char)c)) {
      ncr.append((char)c);
      c = in.read();
    }
    if (c != -1 && c != ';' && !Character.isSpace((char)c)) {
      in.unread(c);
    }
    int value = Integer.parseInt(ncr.toString());
    if (value > 255) {
      return '?';
    }
    return (char)value;
  }

  private char readGER(int c) throws IOException
  {
    StringBuffer ger = new StringBuffer();
    readName(c, ger);
    c = in.read();
    if (c != -1 && c != ';' && !Character.isSpace((char)c)) {
      in.unread(c);
    }
    if (ger.toString().equals("copy")) {
      return (char)169;
    } 
    else if (ger.toString().equals("reg")) {
      return (char)174;
    } 
    else if (ger.toString().equals("amp")) {
      return (char)38;
    }
    else if (ger.toString().equals("gt")) {
      return (char)62;
    } 
    else if (ger.toString().equals("lt")) {
      return (char)60;
    } 
    else if (ger.toString().equals("quot")) {
      return (char)34;
    }   
    else if (ger.toString().equals("nbsp")) {
      return (char)160;
    }
    else {
      return '?';
    }
  }

  private void readCommentDecl() throws IOException
  {
    readComment();
    int c;
    while ((c = in.read()) != -1) {
      if (c == '>') {
        return;
      }
      else if (c == '-') {
        c = in.read();
        if (c == -1) {
//          System.out.println("Unexpected end of file in comment declaration");
          return;
        }
        else if (c == '-') {
          readComment();
        }
        else {
/*
          System.out.println("Illegal character(s) in comment declaration: "
                             + '-' + (char)c);
*/
        }
      }
      else if (!Character.isSpace((char)c)) {
/*
        System.out.println("Illegal character(s) in comment declaration: "
                           + (char)c);
*/
      }
    }
//    System.out.println("Unexpected end of file in comment declaration");
  }

  private void readComment() throws IOException
  {
    int c;
    while ((c = in.read()) != -1) {
      if (c == '-') {
        c = in.read();
        if (c == -1) {
//          System.out.println("Unexpected end of file in comment");
          return;
        }
        else if (c == '-') {
          return;
        }
      }
    }
//    System.out.println("Unexpected end of file in comment");
  }

  private int readEndTag(int c) throws IOException
  {
    tag.start = false;
    if (readName(c, tag.name) != 0) {
      return -1;
    }

    while ((c = in.read()) != -1) {
      if (c == '>') {
        return 0;
      }
      else if (!Character.isSpace((char)c)) {
/*
        System.out.println("Illegal character(s) in </" + tag.name + ">: " +
                           (char)c);
*/
      }
    }
//    System.out.println("Unexpected end of file in </" + tag.name + ">");
    return -1;
  }

  private int readStartTag(int c) throws IOException
  {
    tag.start = true;
    if (readName(c, tag.name) != 0) {
      return -1;
    }
    readSpace();
    if ((c = in.read()) == -1) {
//      System.out.println("Unexpected end of file in <" + tag.name + ">");
      return -1;
    }
    return readAttributesValues(c);
  }

  private int readAttributesValues(int c) throws IOException
  {
    while (c != -1) {
      if (c == '>') {    // Close start tag
        return 0;
      }  
      else if (!Character.isLetter((char)c)) {    // Not an attribute name
/*
        System.out.println("Illegal character(s) in <" + tag.name + ">: " +
                           (char)c);
*/
        c = in.read();
        while (c != -1 && !Character.isLetter((char)c) && c != '>') {
          c = in.read();
        }
        if (c == -1) {
//          System.out.println("Unexpected end of file in <" + tag.name + ">");
          return -1;
        }
        else if (c == '>') {    // Not parsed as an attribute
          return 0;
        }
      }

      // Now c is the first letter of the attribute
      tag.attributes.addElement(new StringBuffer());
      StringBuffer attribute = (StringBuffer)(tag.attributes.lastElement());
      if (readName(c, attribute) != 0) {
        return -1;
      }
      
      readSpace();
      c = in.read();
      if (c == -1) {
//        System.out.println("Unexpected end of file in <" + tag.name + ">");
        return -1; 
      }
      else if (c == '>') {    // Add an empty value
        tag.values.addElement(new StringBuffer());
        return 0;
      }   
      else if (c == '=') {    // Now look for literal or name taken
        if (readValue() != 0) {
          return -1;
        }
        readSpace();
        c = in.read();
      }
      else {    // Add an empty value
        tag.values.addElement(new StringBuffer());
      } 
    }
//    System.out.println("Unexpected end of file in <" + tag.name + ">");
    return -1;
  }

  private int readName(int c, StringBuffer name) throws IOException
  {
    name.append((char)c);
    c = in.read();
    while (c != -1 &&
           (Character.isLetterOrDigit((char)c) || c == '-' || c == '.')) {
      name.append((char)c);          
      c = in.read();
    }          
    if (c == -1) {
//      System.out.println("Unexpected end of file in: " + name);
      return -1; 
    }                
    else {
      in.unread(c);
      return 0;
    }
  }

  private int readValue() throws IOException
  {
    readSpace();
    int c = in.read();
    if (c == -1) {
/*
      System.out.println("Unexpected end of file in value: " + 
                         (StringBuffer)tag.attributes.lastElement());
*/
      return -1; 
    }        
    else if (c == '\"' || c == '\'') {    // A literal
      tag.values.addElement(new StringBuffer());
      if (readLiteral(c) != 0) {
        return -1;
      }
      else {
        return 0;
      }
    }
    else if (Character.isLetterOrDigit((char)c) || c == '-' || c == '.') {
      tag.values.addElement(new StringBuffer());
      if ((readName(c, (StringBuffer)(tag.values.lastElement()))) != 0) {
        return -1;
      }
      else {
        return 0;
      }
    }
    else {
      c = in.read();
      while (c != -1 && c != '\"' && c != '\'' && 
             !Character.isLetterOrDigit((char)c) && c != '-' && c != '.') {
        c = in.read();
      }
      if (c == -1) {
/*
        System.out.println("Unexpected end of file in value: " +
                           (StringBuffer)tag.attributes.lastElement());
*/
        return -1; 
      }
      else {
        in.unread(c);
        return readValue();
      }
    }
  }

  // Shoule be able to handle GER and NCR too!
  // What about illegal literal without quote?
  private int readLiteral(int c) throws IOException
  {
    char delimiter = (char)c;
    StringBuffer literal = (StringBuffer)(tag.values.lastElement());
    literal.append(delimiter);
    while ((c = in.read()) != -1 && c != delimiter) {
      literal.append((char)c);
    }
    if (c == -1) {
//      System.out.println("Unexpected end of file in literal: " + literal);
      return -1;      
    }
    literal.append(delimiter);
    return 0;
  }

  private void readSpace() throws IOException
  {
    int c = in.read();
    while (c != -1 && Character.isSpace((char)c)) {
      c = in.read();
    }
    if (c != -1) {
      in.unread(c);
    }
  }
}
