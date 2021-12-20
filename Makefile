.PHONY: all core audio gui

core:
	python3 core/__main__.py

audio:
	cd audio && make run

gui:
	cd gui && python3 -m http.server
