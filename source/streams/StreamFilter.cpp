#include "StreamFilter.h"
#include "ManagedBuffer.h"
#include "DataStream.h"
#include "StreamNormalizer.h"
#include "CodalDmesg.h"

using namespace codal;

StreamFilter::StreamFilter( DataSource &source ) : upStream( source )
{
    reset();
    this->downStream = NULL;
    source.connect( *this );
}

StreamFilter::~StreamFilter()
{
    // NOP
}

ManagedBuffer StreamFilter::pull()
{
    int inFmt = this->upStream.getFormat();
    ManagedBuffer buffer = this->upStream.pull();

    if( this->lpf_enabled )
    {
        uint8_t * datum = buffer.getBytes();

        // LPF: Y(n) = (1-ß)*Y(n-1) + (ß*X(n))) = Y(n-1) - (ß*(Y(n-1)-X(n)));
        for( int i=0; i<buffer.length() / 2; i++ )
        {
            float value = StreamNormalizer::readSample[inFmt]( datum );
            lpf_value = lpf_value - (lpf_beta * (lpf_value - (float)value));
            StreamNormalizer::writeSample[inFmt]( datum, (int)lpf_value );

            datum += 2; // skip a short - should be calculated!
        }
    }

    // Missing function
    // HPF: y[i] := α * (y[i−1] + x[i] − x[i−1])

    if( this->avg_enabled )
    {
        uint8_t * datum = buffer.getBytes();

        for( int i=0; i<buffer.length() / 2; i++ )
        {
            float value = StreamNormalizer::readSample[inFmt]( datum );

            value = (int)value & 0b11111100;

            datum += 2; // skip a short - should be calculated!
        }
    }

    return buffer;
}

int StreamFilter::pullRequest()
{
    if( this->downStream != NULL )
        this->downStream->pullRequest();

    return 0;
}

void StreamFilter::connect( DataSink &sink )
{
    this->downStream = &sink;
}

void StreamFilter::disconnect()
{
    this->downStream = NULL;
}

int StreamFilter::getFormat()
{
    return this->upStream.getFormat();
}

int StreamFilter::setFormat( int format )
{
    return this->upStream.setFormat( format );
}

void StreamFilter::reset()
{
    this->lpf_enabled = false;
    this->lpf_beta = 0.003f;
    this->lpf_value = 0.0f;

    this->hpf_alpha = false;
    this->hpf_alpha = 0.0f;

    this->avg_enabled = false;
    this->avg_value = 0.0f;
}

void StreamFilter::enableFunction( int function )
{
    //
}