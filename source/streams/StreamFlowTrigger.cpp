#include "StreamFlowTrigger.h"
#include "ManagedBuffer.h"
#include "DataStream.h"
#include "CodalDmesg.h"

using namespace codal;

StreamFlowTrigger::StreamFlowTrigger( DataSource &source ) : upStream( source )
{
    this->eventHandler = NULL;
    this->downStream = NULL;
    source.connect( *this );
}

StreamFlowTrigger::~StreamFlowTrigger()
{
    // NOP
}

void StreamFlowTrigger::setDataHandler( void (*handler)(int) )
{
    this->eventHandler = handler;
}

ManagedBuffer StreamFlowTrigger::pull()
{
    (*this->eventHandler)( TRIGGER_PULL );
    return this->upStream.pull();
}

int StreamFlowTrigger::pullRequest()
{
    (*this->eventHandler)( TRIGGER_REQUEST );
    if( this->downStream != NULL )
        return this->downStream->pullRequest();
    
    return DEVICE_BUSY;
}

void StreamFlowTrigger::connect( DataSink &sink )
{
    this->downStream = &sink;
}

bool StreamFlowTrigger::isConnected()
{
    return this->downStream != NULL;
}

void StreamFlowTrigger::disconnect()
{
    this->downStream = NULL;
}

int StreamFlowTrigger::getFormat()
{
    return this->upStream.getFormat();
}

int StreamFlowTrigger::setFormat( int format )
{
    return this->upStream.setFormat( format );
}