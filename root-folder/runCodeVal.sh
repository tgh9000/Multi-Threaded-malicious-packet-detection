apk add valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes -s workspace/skeleton/build/idsniff -i lo -v 1