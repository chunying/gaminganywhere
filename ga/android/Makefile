
MODE	?= debug

all:
	android update project -p .
	ndk-build
	ant $(MODE)

clean:
	android update project -p .
	ant clean
	ndk-build clean
	rm -rf libs obj

