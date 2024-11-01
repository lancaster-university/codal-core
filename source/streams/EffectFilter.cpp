#include "EffectFilter.h"
#include "ManagedBuffer.h"
#include "DataStream.h"
#include "StreamNormalizer.h"
#include "CodalDmesg.h"

using namespace codal;

EffectFilter::EffectFilter(DataSource &source, bool deepCopy) : upStream( source )
{
    this->downStream = NULL;
    this->deepCopy = deepCopy;
    source.connect( *this );
}

EffectFilter::~EffectFilter()
{
}

ManagedBuffer EffectFilter::pull()
{
    ManagedBuffer input = this->upStream.pull();
    ManagedBuffer output = (deepCopy || input.isReadOnly()) ? ManagedBuffer(input.length()) : input;

    applyEffect(input, output, this->upStream.getFormat());
    return output;
}

int EffectFilter::pullRequest()
{
    if( this->downStream != NULL )
        return this->downStream->pullRequest();
    return DEVICE_BUSY;
}

void EffectFilter::connect(DataSink &sink)
{
    this->downStream = &sink;
}

bool EffectFilter::isConnected()
{
    return this->downStream != NULL;
}

void EffectFilter::disconnect()
{
    this->downStream = NULL;
}

int EffectFilter::getFormat()
{
    return this->upStream.getFormat();
}

int EffectFilter::setFormat( int format )
{
    return this->upStream.setFormat( format );
}

float EffectFilter::getSampleRate()
{
    return this->upStream.getSampleRate();
}

/**
 * Defines if this filter should perform a deep copy of incoming data, or update data in place.
 *
 * @param deepCopy Set to true to copy incoming data into a freshly allocated buffer, or false to change data in place.
 */
void EffectFilter::setDeepCopy( bool deepCopy )
{
    this->deepCopy = deepCopy;
}

/**
 * Default effect - a simple pass through filter.
 * 
 * @param inputBuffer the buffer containing data to process.
 * @param outputBuffer the buffer in which to store the filtered data. n.b. MAY be the same memory as the input buffer.
 * @param format the format of the data (word size and signed/unsigned representation)
 */
void EffectFilter::applyEffect(ManagedBuffer inputBuffer, ManagedBuffer outputBuffer, int format)
{
    if (inputBuffer.getBytes() != outputBuffer.getBytes())
        memcpy(outputBuffer.getBytes(), inputBuffer.getBytes(), inputBuffer.length());
}