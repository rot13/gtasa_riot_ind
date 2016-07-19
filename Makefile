all: gtasa_riot_ind.exe

gtasa_riot_ind.exe : gtasa_riot_ind.c
	i686-w64-mingw32-gcc -std=c99 -O3 gtasa_riot_ind.c -o gtasa_riot_ind.exe -s -mwindows
clean:
	rm *.exe
