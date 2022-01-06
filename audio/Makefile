path_to_mp3trim = "/media/exthdd/aporee_trim/mp3_trim"

all: clean build run

build:
	mkdir build
	cmake . -B build
	cd build && make
	ln -s ${path_to_mp3trim} mp3

clean:
	-rm -rf build
	-rm mp3

run:
	cd build && ./AUDIO

mount:
	sudo mount -t ntfs /dev/sda1 /media/exthdd

unmount:
	sudo umount /media/exthdd
