ls; ls -a; ls -l ..;

help > a.out; cat a.out; rm a.out;

sleep 15&; jobs; sleep 10&; kill 0;

kill abc; kill 25;

touch a; mv a b; rm a;

cat -n<wsh.c;

grep ^....$ /usr/share/dict/words   >flw; rm flw;

>a; mv a b; cat a; cat b; rm a b;

< a cat >b -s; rm b;

grep ^[diwas]*$ /usr/share/dict/words | grep ^.....$ | wc -l;

help | grep jobs;

echo Done with testing!;

exit;