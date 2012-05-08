#include <stdio.h>
#include "z_libpd.h"
#include <portaudio.h>
#include <stdint.h>
#include <string.h>

void pdprint(const char *s) {
  printf("%s", s);
}

void pdnoteon(int ch, int pitch, int vel) {
  printf("noteon: %d %d %d\n", ch, pitch, vel);
}

#define BLOCKSIZE_FRAMES 64
#define N_IN_CHANNELS 2
#define N_OUT_CHANNELS 2
#define BLOCKSIZE_FLOATS_IN (BLOCKSIZE_FRAMES*N_IN_CHANNELS)
#define BLOCKSIZE_FLOATS_OUT (BLOCKSIZE_FRAMES*N_OUT_CHANNELS)
#define BLOCKSIZE_BYTES_IN (BLOCKSIZE_FLOATS_IN*sizeof(float))
#define BLOCKSIZE_BYTES_OUT (BLOCKSIZE_FLOATS_OUT*sizeof(float))
#define min(x,y) ((x)<(y)?(x):(y))

float* ringbufInput;
float* ringbufOutput;
volatile uint8_t ringbufReadBlock, ringbufWriteBlock;
uint8_t ringbufCount;
int situation = 0;


static int portaudioCallback( const void* input, void * output,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData )
{
	float *out = (float*)output;
	float *in = (float*)input;


/*	if ( statusFlags & paInputUnderflow )
		printf("input underflow\n");
	if ( statusFlags & paInputOverflow )
		printf("input overflow\n");
	if ( statusFlags & paOutputUnderflow )
		printf("output underflow\n");
	if ( statusFlags & paOutputOverflow )
		printf("output overflow\n");
	if ( statusFlags & paPrimingOutput )
		printf("priming output\n");
*/





//	printf("request %i blocks (w %2i - r %2i / %2i)", framesPerBuffer/BLOCKSIZE_FRAMES, ringbufWriteBlock, ringbufReadBlock, ringbufCount );
	while( framesPerBuffer> 0 )
	{
		// count is the number of buffers -- either the number remaining in the ring buffer, or 
		int count = min(framesPerBuffer/BLOCKSIZE_FRAMES,ringbufCount-ringbufReadBlock);
		float *pdIn = ringbufInput+BLOCKSIZE_FLOATS_IN*ringbufReadBlock;
		float *pdOut = ringbufOutput+BLOCKSIZE_FLOATS_OUT*ringbufReadBlock;
//		printf(" -> count %i", count );

		framesPerBuffer -= BLOCKSIZE_FRAMES*count;
		while (count > 0 )
		{
//			printf(" [%i] copying in %i, out %i\n", count, ringbufReadBlock, ringbufReadBlock );
			memcpy( pdIn, in, BLOCKSIZE_BYTES_IN );
			memcpy( out, pdOut, BLOCKSIZE_BYTES_OUT );
			in += BLOCKSIZE_FLOATS_IN;
			pdIn += BLOCKSIZE_FLOATS_IN;
			out += BLOCKSIZE_FLOATS_OUT;
			pdOut += BLOCKSIZE_FLOATS_OUT;
			ringbufReadBlock++;
			count--;
			situation --;
		}

		if ( ringbufReadBlock >= ringbufCount )
		{
			ringbufReadBlock = 0;
		}

	}
//	printf("\n");

	return 0;
}


// check

static void processBlock(float* in, float* out)
{
	libpd_process_float( 1, in, out );
}
void usage( const char* argv0 )
{
	fprintf(stderr, "usage: %s [-l libpath [-l libpath] ...] [-s samplerate] [-n bufcount] [-np portaudio_bufcount] file\n", argv0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	int sRate = 44100;
	ringbufCount = 16;
	int portaudioBufcount = ringbufCount/4;
	char *file;
	if ( argc > 2 )
	{	
		for ( int i=1; i<argc-1; i++ )
		{
			if ( strcmp(argv[i], "-s" ) == 0 )
			{
				sRate = atoi( argv[++i] );
			}
			else if ( strcmp( argv[i], "-n" ) == 0 )
			{
				ringbufCount = atoi(argv[++i]);
			}
			else if ( strcmp( argv[i], "-np" ) == 0 )
			{
				portaudioBufcount = atoi(argv[++i]);
			}	
			else if ( strcmp( argv[i], "-l" ) == 0 )
			{
				libpd_add_to_search_path( argv[++i] );
			}
			else
			{
				usage(argv[0]);
				return -1;
			}

		}
	}
	file = argv[argc-1];
	
	size_t inbufSizeBytes  = sizeof(float)*ringbufCount*BLOCKSIZE_FLOATS_IN;
	size_t outbufSizeBytes = sizeof(float)*ringbufCount*BLOCKSIZE_FLOATS_OUT;
	ringbufInput = (float*)malloc( inbufSizeBytes );
	ringbufOutput = (float*)malloc( outbufSizeBytes );
	memset( ringbufInput, 0, inbufSizeBytes );
	memset( ringbufOutput, 0, outbufSizeBytes );
	ringbufReadBlock =0;
	ringbufWriteBlock =0;
	printf("sample rate %i, %i buffers (%i frames), opening %s\n", sRate, ringbufCount, ringbufCount*BLOCKSIZE_FRAMES, file);

	// init pd
	libpd_printhook = (t_libpd_printhook) pdprint;
	libpd_noteonhook = (t_libpd_noteonhook) pdnoteon;
	libpd_init();
	libpd_init_audio(2, 2, sRate);
	addacs_in_setup();
	addacs_out_setup();

	// compute audio    [; pd dsp 1(
	libpd_start_message(1); // one entry in list
	libpd_add_float(1.0f);
	libpd_finish_message("pd", "dsp");

	// open patch    o   [; pd open file folder(
	char folder[4096];
	char* basename = strrchr( file, '/' );
	if ( basename == NULL )
	{
		strcpy( folder, "" );
	}
	else
	{
		strncpy( folder, file, basename-file ); 
		file = basename+1;
	}
	printf(" opening %s/%s\n"	, folder, file );
	libpd_openfile(file, folder);
	printf(" addacs_in exists? %i\n", libpd_exists( "addacs_in" ) ); 


	PaStream *stream;
	PaError err;

	err = Pa_Initialize();
	if ( err != paNoError )
	{
		fprintf(stderr, "Pa_Initialize failed: %i %s\n", err, Pa_GetErrorText( err ) );
		exit(1);
	}

	err = Pa_OpenDefaultStream( &stream, 
			N_IN_CHANNELS, 
			N_OUT_CHANNELS, 
			paFloat32,
			sRate,
			portaudioBufcount*BLOCKSIZE_FRAMES,
			portaudioCallback,
			NULL );
	if ( err != paNoError )
	{
		fprintf(stderr, "Pa_Initialize failed: %i %s\n", err, Pa_GetErrorText( err ) );
		exit(2);
	}

	err = Pa_StartStream( stream );
	if ( err != paNoError )
	{
		fprintf(stderr, "Pa_Initialize failed: %i %s\n", err, Pa_GetErrorText( err ) );
		exit(3);
	}


	int loopcount =0;
	int needcount[128];

	int sleepMicros = 1000*1000*BLOCKSIZE_FRAMES/sRate;
	while( 1 )
	{
		int countNeeded;
		// have to be careful here as we can be interrupted
		volatile int ringbufReadBlockCached = ringbufReadBlock;
		if ( ringbufReadBlockCached >= ringbufWriteBlock )
		{
			countNeeded = ringbufReadBlockCached - ringbufWriteBlock;
		}
		else
		{
			countNeeded = ringbufReadBlockCached+ringbufCount - ringbufWriteBlock;
		}


		if( countNeeded >=0 && countNeeded < 128 )
			needcount[countNeeded]++;		
		loopcount++;
		if ( loopcount%10000==0 )
		{
			printf( "\n %8i %8i %8i %8i %8i %8i %8i %8i\n", 0, 1, 2, 3, 4, 5, 6, 7 );
			for ( int i=0; i<64; i++ )
			{
				printf(" %8i", needcount[i] );
				if ( i % 8 == 7 )
					printf("\n");
			}
			printf("\n");
		}

//		printf("need %i (w %2i - r %2i) (situation % 8i)\r", countNeeded, ringbufWriteBlock, ringbufReadBlockCached, situation );
		while ( countNeeded > 0 )
		{
			if ( ringbufWriteBlock >= ringbufCount )
				ringbufWriteBlock = 0;
			int whichInBlock = ringbufWriteBlock;
			int whichOutBlock = ringbufWriteBlock;

			float* in = ringbufInput+whichInBlock*BLOCKSIZE_FLOATS_IN;
			float *out = ringbufOutput+whichOutBlock*BLOCKSIZE_FLOATS_OUT;
			processBlock( in, out );
			situation++;



			whichInBlock++;
			whichOutBlock++;
			ringbufWriteBlock++;
			countNeeded--;
			if ( whichInBlock >= ringbufCount )
				whichInBlock = 0;
			if ( whichOutBlock >= ringbufCount )
				whichOutBlock = 0;
		}

		usleep( sleepMicros/4 );
	}

	Pa_StopStream( stream );
	Pa_CloseStream( stream );

	Pa_Terminate();

	free( ringbufInput );
	free( ringbufOutput );

	return 0;

}

