cd cs241podman
podman build -t alpinebuild .
cd ..
podman run -it --rm --cap-add=NET_RAW -v ./root-folder:/root alpinebuild

#    podman exec -it containername /bin/bash