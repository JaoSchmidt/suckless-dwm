#!/bin/bash
# (for pulseaudio users)
# This script parses the output of `pacmd list-sinks' to find volume and mute
# status of the default audio sink and whether headphones are plugged in or not
# Also see ../daemons/pulse_daemon.sh
pacmd list-sinks | awk '
    BEGIN {
        ICONsn ="  \" \\uf028 \"  " # headphone unplugged, not muted
        ICONsm ="  \" \\uf026 \"  " # headphone unplugged, muted
        ICONhn ="  \" \\uf6a9 \"  " # headphone plugged in, not muted
        ICONhm ="  \" \\uf027 \"  " # headphone plugged in, muted

    }
    f {
        if ($1 == "muted:" && $2 == "yes") {
            m = 1
        } else if ($1 == "volume:") {
            if ($3 == $10) {
                vb = $5
            } else {
                vl = $5
                vr = $12
            }
        } else if ($1 == "active" && $2 == "port:") {
            if (tolower($3) ~ /headphone/)
                h = 1
            exit
        }
        next
    }
    $1 == "*" && $2 == "index:" {
        f = 1
    }
    END {
        if (f) {
				if (h)
					if(m)
						system("/usr/bin/printf \"\\uf026 \"")
					else
						if(vb < 60)
							system("/usr/bin/printf \"\\uf027 \"")
						else
							system("/usr/bin/printf \"\\uf028 \"")
				else
					system("/usr/bin/printf \"\\uf6a9 \"")

            if (vb)
                print vb
            else
                printf "L%s R%s\n", vl, vr
        }
    }
'
