/root/workspace/skeleton/build/idsniff -i interface -v 1
/root/workspace/skeleton/build/idsniff -i eht0
/root/workspace/skeleton/build/idsniff -i interface lo
/root/workspace/skeleton/build/idsniff -i lo
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
/root/workspace/skeleton/build/idsniff -i lo -v 1
exit
ls
cd root/
ls
cd workspace/
ls
cd skeleton/
ls
cd src
ls
make
make
cd ..
ls
cd build
ls
cd ..
cd src
make clean
make
cd ..
cd build
./idsniff
cd ..
cd ..
cd ..
cd workspace/
cd skeleton/]
cd skeleton/
cd src/
make clean
make 
make clean
make 
/root/workspace/skeleton/build/idsniff -i lo -v 1
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
make clean
ls
cd root/
cd workspace/
cd skeleton/
cd src/
make clean
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
cd ~
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
cd ..
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
cd root
cd workspace/
/skeleton/build/idsniff -i eth0 -v 1
ls
cd skeleton/
ls
cd src/
make
cd ~
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
make
ls 
cd workspace/
ls
cd skeleton/
ls
cd src/
make
make
make
make
cd ~
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
ls
cd r
cd root/
ls
cd workspace/
cd skeleton/
cd src/
make clean
make
make
make
make
make
make
make
make
make clean
make
make clean
make
make clean
make
ls ~
cd ~
cd ..
container). If you're in the lab, the host is the physical machine that you're sitting at. If you're logged in remotely, you can consider the host as the machine that you're connected to (i.e., remote-##.dcs.warwick.ac.uk) 
Creating the container
To get started, make a folder called cs241podman and create a file called Dockerfile with the following content:
FROM alpine:3.22.2
RUN apk add --no-cache build-base bash libpcap-dev python3 scapy wget curl
RUN apk add --no-cache hping3 --repository https://dl-cdn.alpinelinux.org/alpine/edge/testing
ENTRYPOINT ["/bin/bash"]
This is a script that will create a basic Alpine Linux installation containing:
build-base: tools for compiling C programs
bash: the bash shell – slightly easier to work with than the plain /bin/sh shell
libpcap-dev: the development files for libpcap
Scapy: Scapy is a Python library for creating/sending packets
hping3: a tool to create/send individual packets, this is from the testing repository
wget and curl: command-line tools for sending HTTP requests
Create your container image by running (from within your cs241podman folder):
podman build -t alpinebuild .
(note the . on the end, this is important – it specifies that you want to build the container from the current directory)
Now lets make some folders for the code we'll be working with (do this outside of your cs241podman folder):

mkdir -p cs241coursework/root-folder/workspace
Inside root-folder, make a file called .bashrc with the following content:

cd ~
PS1="(container)\h:\w\$ "  
Running the container
Make sure your current directory is cs241coursework. You can now run an instance of the container with the following command:

podman run -it --rm --cap-add=NET_RAW -v ./root-folder:/root alpinebuild
You'll be using that command a lot, so it would be a good idea for you to put that command in a file called 'runContainer.sh' (e.g. in your cs241coursework folder), then you can run the script whenever you want to start a container.
Note that any content you put in the root-folder (on the host) will be shared with the container, and available within /root of the container. Any changes you make will be available to the host (this also means that changes will persist after the container has finished). Any changes you make to the container outside of /root will be lost as soon as the container exits - this is because the above command contains --rm, which will delete the container instance as soon as you exit it (with the exit command, or pressing Ctrl+d). In most cases, this should be fine, because you can just launch another container and know that it will have exactly the same functionality as the original container image.
If you want to change the original build (e.g. to add extra utilities), you can modify the Dockerfile and rebuild the container image.
Running a server in a container
Let's run a very basic web server in the container so we can become familiar with how networking works in the container. On the host, make a folder called 'webpages' within your root-folder. Use whatever editor you like (e.g. Visual Studio Code) to make a file here called index.html, and put some content in there such as:

<html><body><p>This is a test file</p></body></html>
Now, go to the container, and cd into your /root/webpages folder (run ls, you should see your index.html file) and run:

python3 -m http.server
This will make python run a basic web server (it's only ever used for testing purposes - you wouldn't use this for an actual production system).

Now, keep that terminal open, and in another terminal (if you're doing the lab remotely, you'll need to do ssh in a new window again) let's connect to the same container so we can run a different command at the same time - in your new terminal type:
podman exec -it containername /bin/bash
Where, containername is the 12-digit hexadecimal string that is displayed in your prompt of the container (in your other window). Or you can just use -l to connect to the container that you started most recently.
This should give you a new shell in the same container, so you can access the http server you launched. Let's use nc to do that - in the new window run:

nc localhost:8000
This initiates a tcp connection to port 8000. Now, you can send an HTTP request by typing:

GET /index.html
press enter (twice), and the server should respond with the contents of the file. Press Ctrl+d to quit nc.

Run nc localhost:8000 again, but this time try typing something that isn't a valid request – you should get a 'Bad request' message back.
Try using wget and curl to send requests to your network server:
wget http://localhost:8000/index.html
curl http://localhost:8000/index.html
Now try modifying last week's tcp client to connect to port 8000 and send a GET request, put the files inside your root-folder (e.g., root-folder/tcp-http), then try building it from within the container (the container should be able to find the file within /root/tcp-http).

Getting started with the skeleton code
You can now unzip the skeleton.zip file into your root-folder/workspace folder.

Then you should be able to go into the container and build the source by changing directory into /root/workspace/skeleton/src then run

make
If you ever need to rebuild the project from scratch, you can delete all compiled files by running

make clean
To run the code, you can run

/root/workspace/skeleton/build/idsniff -i interface
(replace interface with either eth0 or lo). This will start a packet capturing session at the specified interface. If no interface is specified, i.e., if the -ioption is not used, then the program will assume the default interface name eth0 for the interface. The program will continue to run unless you kill it by pressing Ctrl+c, i.e., by sending the SIGINT signal to it.

The skeleton code will not show any captured packets unless you run it with the -v option to set the verbose flag to 1 as shown below

/root/workspace/skeleton/build/idsniff -i eth0 -v 1




/root/workspace/skeleton/build/idsniff -i eth0 -v 1
make clean
make 
cd root
cd workspace/
cd skeleton/
cd src/
make clean
make
cd ~
cd ..
/root/workspace/skeleton/build/idsniff -i eth0 -v 1
ls
cd root/
ls
ls
ls
ls
./gotocode.sh
chmod gotocode.sh +x
chmod +x gotocode.sh 
./gotocode.sh
ls
./gotocode.sh
cd workspace/
cd skeleton/src/
make clean
make
cd ~
cd ..
ls
cd root/
ls
ls
./runCode.sh
chmod +x runCode.sh 
./runCode.sh
cd root
ls
./gotocode.sh 
cd workspace/skeleton/src
make clean
make
cd ~
ls
./runCode.sh 
ls
./runCode.sh 
./runCode.sh 
/workspace/skeleton/build/idsniff -i eth0 -v 1
exit
ls
cd root/
ls
./runCode.sh 
ls
cd workspace/
cd skeleton/
cd src/
make
make clean
make
cd ~
ls
./runCode.sh 
cd workspace/skeleton/build/
ls
lsidsniff -i eth0 -v 1
idsniff -i eth0 -v 1
/idsniff -i eth0 -v 1
cd ../../..
exit
cd root/
ls
./runCode.sh 
ls
cd workspace/skeleton/src/
cd ../build/
ls
pwd
cd ~
cd root
ls
cd ..
ls
cd
./runCode.sh 
./runCode.sh 
ls
more runCode.sh 
ls
pwd
ls -l /root/workspace/skeleton/build/idsniff 
ls -l workspace/skeleton/build/idsniff 
ls
./runCode.sh 
./runCode.sh eth0
./runCode.sh lo
ls
cd root/
ls
./runCode.sh eth0
exit
ls
cd root
ls
./gotocode.sh 
ls
cd workspace/skeleton/src/
cd ../../..
./gotocode.sh 
ls
./makeCode.sh
chmod +x makeCode.sh 
./makeCode.sh
./makeCode.sh
./makeCode.sh
./makeCode.sh
./makeCode.sh
./makeCode.sh
./makeCode.sh
./makeCode.sh
./makeCode.sh
ls
./runCode.sh 
./runCode.sh 
./runCode.sh eth0
ls
./makeCode.sh 
./makeCode.sh 
./runCode.sh 
./runCode.sh eth0
ls
cd root/
ls
./runCode.sh 
./runCode.sh eth0
cd root/
ls
./makeCode.sh 
./runCode.sh 
cd root/
./makeCode.sh q
./runCode.sh 
./runCode.sh eth0
cd root/
ls
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./runCode.sh 
./runCode.sh etho0
./runCode.sh eth0
./makeCode.sh 
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
ls
./makeCode.sh 
./runCode.sh lo
./makeCode.sh 
./makeCode.sh 
./runCode.sh lo
./makeCode.sh 
./runCode.sh lo
./makeCode.sh 
./runCode.sh lo
./runCode.sh lo
./runCode.sh lo
./runCode.sh eth0
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./runCode.sh eth0
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh lo
./runCode.sh eth0
./runCode.sh eth0
./runCode.sh eth0
./makeCode.sh 
./makeCode.sh 
./runCode.sh eth0
./runCode.sh eth0
exit
/q
\q
./makeCode.sh 
cd root/
ls
ls
./makeCode.sh 
./makeCode.sh     wget --no-hsts www.facebook.com
    wget --no-hsts www.facebook.com
wget --no-hsts www.google.co.uk
wget --no-hsts www.google.co.uk
wget --no-hsts www.google.co.uk
wget --no-hsts www.google.co.uk
wget --no-hsts www.google.co.uk
wget --no-hsts www.google.co.uk
    wget --no-hsts www.facebook.com
    wget --no-hsts www.facebook.com
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 1000000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 100000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 100000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 1000000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
cd root/
ls
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
cd root/
ls
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
hping3 -c 10 -d 120 -S -w 64 -p 80 -i u100 localhost
hping3 -c 10 -d 120 -S -w 64 -p 80 -i u100 localhost
hping3 -c 10 -d 120 -S -w 64 -p 80 -i u100 localhost
hping3 -c 100000 -d 120 -S -w 64 -p 80 -i u10 --rand-source localhost
cd root/
ls
./sendArp.sh 
./sendArp.sh 
./sendArp.sh 
    wget --no-hsts www.google.co.uk
wget --no-hsts www.facebook.com
    wget --no-hsts www.google.co.uk
wget --no-hsts www.facebook.com
ls
ls
exit
cd root/
ls 
./runCode.sh 
./runCode.sh lo
./makeCode.sh 
./runCode.sh lo
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./runCode.sh lo
./runCode.sh lo
./runCode.sh lo
./runCode.sh eho0
./runCode.sh eth0
./makeCode.sh 
./runCode.sh eth0
./makeCode.sh 
./runCode.sh lo
./runCode.sh 
exit
cd root/
ls
./runCodeVal.sh 
sudo apt update
valgrind --version
./runCodeVal.sh 
dnf install valgrind
exit
cd root/
ls
./runCodeVal.sh 
cd root
ls
cd..
cd ..
ls
apt update
ls
cd root
ls
./runCodeVal.sh 
./runCodeVal.sh 
./runCodeVal.sh 
cd root/
ls
./makeCode.sh 
./runCode.sh 
./runCode.sh lo
cd root/
ls
./runCode.sh 
apk update
apk add valgrind
./runCode.sh 
./runCode.sh 
./makeCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./makeCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./runCode.sh lo
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./runCode.sh 
./runCode.sh 
./makeCode.sh 
./runCode.sh 
./runCode.sh l0
./runCode.sh l0
./runCode.sh lo
./runCode.sh lo
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
hping3 -c 10000 -d 120 -S -w 64 -p 80 -i u100 --rand-source localhost
ls
cd root/
ls
./sendArp.sh 
    wget --no-hsts www.facebook.com
    wget --no-hsts www.google.co.uk
./sendArp.sh 
./sendArp.sh 
./sendArp.sh 
./sendArp.sh 
./sendArp.sh 
./sendArp.sh 
