#ifdef __cplusplus
    extern "C" {
#endif

enum cameraError {
	cameraE_Ok,

	cameraE_Failed,

	cameraE_InvalidParam,
	cameraE_UnknownDevice,
	cameraE_PermissionDenied,
};

struct imageBuffer {
	void*			lpBase;
	size_t			sLen;
};

struct histogramBuffer {
    size_t          sLen;
    double          dValues[];
};

struct imgRawImage {
	unsigned int numComponents;
	unsigned long int width, height;

	unsigned char* lpData;
};

struct rectBound {
    unsigned long int xMin;
    unsigned long int xMax;

    unsigned long int yMin;
    unsigned long int yMax;
};


#ifdef __cplusplus
    } /* extern "C" { */
#endif
