
proc TimeChart_resize { frame } {
    global TimeChart_all
    upvar #0 $TimeChart_all($frame) struct

    update
    set width  [winfo width  $struct(frame).canvas]
    set height [expr [winfo height $struct(frame).canvas] - 10]
    set struct(xscale) [expr double($width)/$struct(numberOfIntervals)]

    if { $struct(maxYValue) == 0 } {
	set struct(yscale) 0
    } else {
	set struct(yscale) [expr double($height)/$struct(maxYValue)]
    }
}


proc TimeChart_done { frame } {
    global TimeChart_all
    upvar #0 $TimeChart_all($frame) struct
    unset TimeChart_all($frame)

    foreach plotStructName $struct(allPlots) {
	upvar #0 $plotStructName plotStruct
	unset plotStruct
    }
    if { $struct(afterID) != "" } { after cancel $struct(afterID) }
    unset struct
}


proc TimeChart_new_field { frame unit field } {
    global TimeChart_all
    set TimeChart_all($frame) TimeChart_struct_$frame
    upvar #0 $TimeChart_all($frame) struct

    set struct(updateInterval)    500
    set struct(numberOfIntervals) 120
    set struct(maxYValue)         0
    set struct(allPlots)          { }
    set struct(frame)             $frame

    canvas $struct(frame).canvas -relief sunken
    pack   $struct(frame).canvas -expand 1 -fill both
    bind   $struct(frame).canvas <Configure> "TimeChart_resize $frame"
    pack   $struct(frame) -expand 1 -fill both
    bind   $struct(frame) <Destroy> "TimeChart_done $frame"

    TimeChart_resize $struct(frame)
    set struct(afterID) ""
    TimeChart_periodic_update $frame
}


proc TimeChart_update_field { frame unit field value sender } {
    # value has the format
    # plot_name=value,color

    global TimeChart_all
    upvar #0 $TimeChart_all($frame) struct

    set plot  ""
    set val   ""
    set color ""
    if { ![regexp {([^=]*)=([^,]*),(.*)} $value dummy plot val color] } return

    set plotStructName TimeChart_plotStruct_${frame}_$plot
    upvar #0 $plotStructName plotStruct

    if { [lsearch -exact $struct(allPlots) $plotStructName] == -1 } {
	lappend struct(allPlots) $plotStructName
	set plotStruct(history) { }
	
	for {set idx 0} { $idx < $struct(numberOfIntervals) } { incr idx } {
	    lappend plotStruct(history) 0
	}
	
	$struct(frame).canvas create line 0 0 0 0 -tag TimeChart_line
    }

    set plotStruct(value)   $val
    set plotStruct(updated) 1
    set plotStruct(color)   $color

    if { $struct(maxYValue) < $val } {
	set struct(maxYValue) $val
	TimeChart_resize $frame
    }
}


proc TimeChart_periodic_update { frame } {
    global TimeChart_all
    upvar #0 $TimeChart_all($frame) struct

    if { ![info exist struct] } { return }
    $struct(frame).canvas delete TimeChart_line

    set newMax 0
    foreach plotStructName $struct(allPlots) {
	upvar #0 $plotStructName plotStruct
	set plotStruct(history) \
		"[lrange $plotStruct(history) 1 end] $plotStruct(value)"
	set plotStruct(updated) 0

	set points { }
	set x 0
	set height [expr [winfo height $struct(frame).canvas] - 5]
	foreach y $plotStruct(history) {
	    if { $newMax < $y } { set newMax $y }

	    set y [expr int($height - ($y * $struct(yscale)))]
	    set points "$points [expr int($x)] $y"
	    set x [expr $x + $struct(xscale)]
	}
	eval $struct(frame).canvas create line $points -tag TimeChart_line \
		-fill $plotStruct(color)
    }

    if { $newMax < $struct(maxYValue) } { 
	set struct(maxYValue) $newMax
	TimeChart_resize $frame 
    }

    set struct(afterID) [after $struct(updateInterval) \
	    "TimeChart_periodic_update $frame"]
}


proc TimeChart { } {
    # the format of the value field for this type of message is:
    #    plot-name=current-plot-value,plot-color
    # remember! no spaces allowed
    proc new_field { frame unit field } {
	TimeChart_new_field $frame $unit $field
    }

    proc update_field { frame unit field value sender } {
	TimeChart_update_field $frame $unit $field $value $sender
    }
}

