path_to_mp3trim = "/media/exthdd/aporee_trim/mp3_trim"

all: clean build run

build:
	mkdir build
	cmake . -B build
	cd build && make
	ln -s ${path_to_mp3trim} mp3

	# mkdir mp3
	# sshfs vthevenin@10.8.0.11:aporee/mp3_trim mp3

clean:
	-rm -rf build
	-rm mp3

	# sudo umount -f mp3

run:
	cd build && ./TEST

mount:
	sudo mount -t ntfs /dev/sda1 /media/exthdd

unmount:
	sudo umount /media/exthdd
