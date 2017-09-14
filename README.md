This is an ISDB-T receiver that uses Linux DVB v5 API and channel table for ITU Region 2 (Americas).
It should work for all the countries in Latin America that uses the ISDB-T International (also known as ISDB-Tb, SATVD and SBTVD) stardard.

Author: Rafael Diniz <rafael@riseup.net>

I am grateful to the authors of this program.  
I adjusted it to suit the terrestrial digital broadcasting in Japan.  
And changed to be able to specify the adapter number.
~~~
# Adapter 0, Channel 22, output file test.ts
$ isdbt-capture -a 0 -c 22 -o test.ts

# Adapter 1, Channel 23, output to stdout
$ isdbt-capture -a 1 -c 23 -o -
~~~
Author: airwhite <airwhite@airwhite.net>
