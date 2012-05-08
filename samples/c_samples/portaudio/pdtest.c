#include <stdio.h>
#include "z_libpd.h"
#include <portaudio.h>

void pdprint(const char *s) {
  printf("%s", s);
}

void pdnoteon(int ch, int pitch, int vel) {
  printf("noteon: %d %d %d\n", ch, pitch, vel);
}

static int portaudioCallback( const void* input, void * output,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData )
{
	float *out = (float*)output;
	float *in = (float*)input;

	libpd_process_float( framesPerBuffer/64, in, out );
	return 0;
}


int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s [-l libpath [-l libpath] ...] [-s samplerate] [-n bufcount] file\n", argv[0]);
		return -1;
	}

	int sRate = 44100;
	int bufSize = 2048;
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
				bufSize = 64*atoi(argv[++i]);
			}
			else if ( strcmp( argv[i], "-l" ) == 0 )
			{
				libpd_add_to_search_path( argv[++i] );
			}
			else
			{
				fprintf(stderr, "usage: %s [-s samplerate] [-n bufcount] file\n", argv[0]);
				return -1;
			}

		}
	}
	file = argv[argc-1];
	printf("sample rate %i, buffer size %i, opening %s\n", sRate, bufSize, file);

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
			2, 
			2, 
			paFloat32,
			sRate,
			bufSize,
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


	Pa_Sleep( 60*1000 );

	Pa_StopStream( stream );
	Pa_CloseStream( stream );

	Pa_Terminate();

	return 0;

}

