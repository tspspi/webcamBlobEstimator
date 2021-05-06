CCOBJ=clang -I/usr/local/include/ -Wall -ansi -std=c99 -pedantic -c
CCLINK=clang
CCLINKSUFFIX=-L/usr/local/lib -ljpeg
OBJ=tmp/webcamBlobEstimator.o

bin/webcamBlobEstimator: $(OBJ)

	$(CCLINK) -o bin/webcamBlobEstimator $(OBJ) $(CCLINKSUFFIX)

tmp/webcamBlobEstimator.o: src/webcamBlobEstimator.c src/webcamBlobEstimator.h

	$(CCOBJ) -o tmp/webcamBlobEstimator.o src/webcamBlobEstimator.c
