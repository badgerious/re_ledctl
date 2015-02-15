re_ledctl
=========

This is a simple program for changing the ethernet port LED settings on a Realtek 8111E,
as found on the PC Engines APU1D. I wrote this because I found the default LED settings
to be annoying (By default, when negotiated at 1G, the green LED blinks only on activity,
making it hard to tell if the link is up. When in 100M mode, however, the amber LED shines
steady and the green LED blinks).

This program has been by no means thoroughly tested; I've tested it to the
point that it satisfies my needs. Use it at your own risk.

Building
--------

I wrote this program for FreeBSD, since that's what I run on my APU1D. On FreeBSD, simply clone
and ``make``. Other OSes would require adaptation.

Usage
-----

The Realtek 8111E can support 3 LEDs (the APU1D makes use of only 2; the green LED on the left
is LED 0, the amber LED on the right is LED 1). The LEDs can be configured to light up based
on link speed (10M, 100M, or 1000M). They can also be configured to blink on activity (link speed
light and blinking can be combined).

Here is an example of how to use `re_ledctl`. This configures the LEDs to my preferred setting:

- At 1G, the green LED lights up and blinks on activity. The amber LED is dark.
- At 100M, the amber LED lights up and blinks on activity. The green LED is dark.
- At 10M, same as 100M.

```shell

# Set re0 LED 0 (green LED) to blink (-b) and light up at 1G (-s1000).
re_ledctl -b -s1000 re0 0

# Set re0 LED 1 (amber LED) to blink (-b) and light up at 100M (-s100) and 10M (-s10).
re_ledctl -b -s10 -s100 re0 1

```

