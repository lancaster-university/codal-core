#include "LowPassFilter.h"
#include "CodalDmesg.h"

using namespace codal;

LowPassFilter::LowPassFilter( DataSource &source, float beta, bool deepCopy) : EffectFilter( source, deepCopy )
{
    this->lpf_value = 1.0;
    setBeta(beta);
}

LowPassFilter::~LowPassFilter()
{
}

/**
 * Apply a simple low pass filter on the give buffer of data.
 * Y(n) = (1-ß)*Y(n-1) + (ß*X(n))) = Y(n-1) - (ß*(Y(n-1)-X(n)));
 * 
 * @param inputBuffer the buffer containing data to process.
 * @param outputBuffer the buffer in which to store the filtered data. n.b. MAY be the same memory as the input buffer.
 * @param format the format of the data (word size and signed/unsigned representation)
 */
void LowPassFilter::applyEffect(ManagedBuffer inputBuffer, ManagedBuffer outputBuffer, int format)
{
    if( inputBuffer.length() < 1 )
        return;
    
    int bytesPerSample = DATASTREAM_FORMAT_BYTES_PER_SAMPLE(format);
    int sampleCount = inputBuffer.length() / bytesPerSample;
    uint8_t *in = inputBuffer.getBytes();
    uint8_t *out = outputBuffer.getBytes();

    for( int i=0; i<sampleCount; i++)
    {
        int value = StreamNormalizer::readSample[format]( in );
        lpf_value = lpf_value - (lpf_beta * (lpf_value - (float)value));
        //lpf_value = value & 0xFE; // Strips the last few bits...
        StreamNormalizer::writeSample[format]( out, (int)lpf_value );

        in += bytesPerSample; 
        out += bytesPerSample; 
    }
}

/**
 * Define the Beta value for the filter.
 * This allows the reactiveness of the filter to be controlled.
 *
 * @param beta The beta coefficiant for the filter, in the range 0...1.0f.
 * The lower the value, the more aggresive the filter becomes at filtering higher frequencies.
 */
void LowPassFilter::setBeta( float beta )
{
    this->lpf_beta = beta;
}