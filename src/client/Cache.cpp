/////////////////////////////////////////
//
//   Auxiliary Software class library
//
/////////////////////////////////////////


// Cache system
// Created 7/11/01
// By Jason Boettcher


#include "defs.h"
//#include "corona.h"
#include "Sounds.h"

CCache		*Cache = NULL;



//////////////////////////////////////
//	      The cache item class
//////////////////////////////////////


///////////////////
// Load an image
SDL_Surface *_LoadImage(char *filename)
{
    /*SDL_Surface *psSurf = NULL;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Rmask = Gmask = Bmask = Amask = 0;

    if ( SDL_BYTEORDER == SDL_LIL_ENDIAN ) {
	    Rmask = 0x000000FF;
		Gmask = 0x0000FF00;
		Bmask = 0x00FF0000;
		Amask = 0xFF000000;
	} else {	    
		Rmask = 0xFF000000;
		Gmask = 0x00FF0000;
		Bmask = 0x0000FF00;
		Amask = 0x000000FF;
	}

    // Load the image (32bpp)
	corona::Image* image = corona::OpenImage(filename,corona::FF_AUTODETECT,corona::PF_R8G8B8A8);

	if(!image)
		return NULL;

    
    psSurf = SDL_CreateRGBSurface(SDL_SWSURFACE, image->getWidth(), image->getHeight(), 32, Rmask, Gmask, Bmask, Amask);
    if( !psSurf )
        return NULL;

    // Copy the data over
    memcpy( psSurf->pixels, image->getPixels(), psSurf->w*psSurf->h*4);

    delete image;

    return psSurf;*/

	char* fname = GetFullFileName(filename);
	if(fname == NULL || fname[0] == '\0')
		return NULL;
		
#ifndef WIN32
	struct stat s;
	if(stat(fname, &s) == 0)
	{
//		printf("_LoadImage(%s): %0.1f kBytes\n", fname, s.st_size / 1024.0f);
		return IMG_Load(fname);
	}
	else
	{
//		printf("_LoadImage(%s): ERROR: cannot stat the file\n", fname);
		return NULL;
	}
#else // WIN32
	return IMG_Load(fname);
#endif
}



///////////////////
// Load an image
SDL_Surface *CCache::LoadImg(char *_file)
{
	Type = CCH_IMAGE;
	
	fix_strncpy(Filename, _file);
     
	// Load the image
	Image = _LoadImage(Filename);

	if(Image)
		Used = true;
	else
		printf("CCache::LoadImg: Error loading file: %s\n",Filename);

	return Image;
}


///////////////////
// Loads an image, and converts it to the same colour depth as the screen (speed)
SDL_Surface *CCache::LoadImgBPP(char *_file, int bpp)
{
	SDL_Surface *img;

	Type = CCH_IMAGE;
	fix_strncpy(Filename, _file);

	// Load the image
	img = _LoadImage(Filename);

	if(img)
		Used = true;
	else {
		//printf("CCache::LoadImgBPP: Error loading file: %s\n", Filename);
		return NULL;
	}

	// Convert the image to the screen's colour depth
	Image = SDL_CreateRGBSurface(SDL_SWSURFACE, img->w,img->h,bpp, 0,0,0,0);

	// Blit it onto the new surface
	SDL_BlitSurface(img,NULL,Image,NULL);
	SDL_FreeSurface(img);

	return Image;
}



///////////////////
// Load a sample
SoundSample* CCache::LoadSample(char *_file, int maxplaying)
{
	Type = CCH_SOUND;
	fix_strncpy(Filename, _file);
	
	char* fullfname = GetFullFileName(Filename);
	if(fullfname == NULL || fullfname[0] == '\0')
	{
		SetError("Error loading sample %s: file not found", _file);
		return NULL;
	}
	
	// Load the sample
	Sample = LoadSoundSample(fullfname, maxplaying);
	
	if(Sample)
		Used = true;
	else
		SetError("Error loading sample: %s",_file);

	return Sample;
}


///////////////////
// Shutdown the cache item
void CCache::Shutdown(void)
{
	if(!Used)
		return;

	switch(Type) {

		// Image
		case CCH_IMAGE:
			if(Image)
				SDL_FreeSurface(Image);
			Image = NULL;
			break;

		// Sample
		case CCH_SOUND:
			if(Sample)
				FreeSoundSample(Sample);
			Sample = NULL;
			break;
	}
}




//////////////////////////////////////
//	      The cache system
//////////////////////////////////////



///////////////////
// Initialize the cache
int InitializeCache(void)
{
	// Allocate the cache
	Cache = new CCache[MAX_CACHE];
	if(Cache == NULL) {
		SystemError("Error: InitializeCache(): Out of memory");
		return false;
	}

	return true;
}


///////////////////
// Shutdown the cache
void ShutdownCache(void)
{
	int n;

	if(Cache == NULL)
		return;


	// Go through the cache and shutdown every item
	for(n=0;n<MAX_CACHE;n++) {
		if(!Cache[n].isUsed())
			continue;

		Cache[n].Shutdown();
	}

	// Free the cache class
	delete[] Cache;
	Cache = NULL;
}



///////////////////
// Load an image
SDL_Surface *LoadImage(char *_filename, int correctbpp)
{
	int n;
	CCache *cach;

	// Go through and see if we can find the same image already loaded
	cach = Cache;
	for(n=0;n<MAX_CACHE;n++,cach++) {
		if(cach->isUsed() && cach->getType() == CCH_IMAGE) {
			if(stricmp(cach->getFilename(),_filename) == 0)
				return cach->GetImage();
		}
	}

	// Didn't find one already loaded? Find a free spot and load one
	cach = Cache;
	for(n=0;n<MAX_CACHE;n++,cach++) {
		if(cach->isUsed())
			continue;
		break;
	}

	// Not enough free spaces
	if(n == MAX_CACHE-1) {
		printf("Error: The cache ran out of free spaces\n");
		return NULL;
	}


	// Load the image
	if(correctbpp > 0)
		return cach->LoadImgBPP(_filename, correctbpp);
	else
		return cach->LoadImg(_filename);
}


///////////////////
// Load a sample
SoundSample* LoadSample(char *_filename, int maxplaying)
{
	int n;
	CCache *cach;

	// Go through and see if we can find the same sound already loaded
	cach = Cache;
	for(n=0;n<MAX_CACHE;n++,cach++) {
		if(cach->isUsed() && cach->getType() == CCH_SOUND) {
			if(stricmp(cach->getFilename(),_filename) == 0)
				return cach->GetSample();
		}
	}

	// Didn't find one already loaded? Find a free spot and load one
	cach = Cache;
	for(n=0;n<MAX_CACHE;n++,cach++) {
		if(cach->isUsed())
			continue;
		break;
	}

	// Not enough free spaces
	if(n == MAX_CACHE-1) {
		printf("Error: The cache ran out of free spaces\n");
		return 0;
	}

	return cach->LoadSample(_filename,maxplaying);
}
