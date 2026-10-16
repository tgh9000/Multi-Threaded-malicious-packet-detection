

#Small scale controlled test
hping3 -c 10 -d 120 -S -w 64 -p 80 -i u100 localhost


#randomised source IP Test
hping3 -c 100000 -d 120 -S -w 64 -p 80 -i u10 --rand-source localhost


hping3 --flood -d 120 -S -w 64 -p 80  localhost