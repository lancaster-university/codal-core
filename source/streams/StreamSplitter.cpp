/*
The MIT License (MIT)

Copyright (c) 2021 Lancaster University.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "CodalConfig.h"
#include "StreamSplitter.h"
#include "ErrorNo.h"
#include "CodalDmesg.h"
#include "Event.h"

using namespace codal;


/**
 * Creates a component that distributes a single upstream datasource to many downstream datasinks
 *
 * @param source a DataSource to receive data from
 */
StreamSplitter::StreamSplitter(DataSource &source, uint16_t id) : upstream(source)
{

    this->id = id;
    this->processed = 0;
    this->numberChannels = 0;
    this->numberAttempts = 0;
    // init array to NULL.
    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        outputChannels[i] = NULL;
    } 
    
    source.connect(*this);
}

/**
 * Callback provided when data is ready.
 */
int StreamSplitter::pullRequest()
{
    if (processed >= numberChannels)
    {
        processed = 0;
        lastBuffer = upstream.pull();

        // For each downstream channel that exists in array outputChannels - make a pullRequest
        for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
        {
            if (outputChannels[i] != NULL){
                outputChannels[i]->pullRequest();
            }
        } 
    }
    else
    {
        numberAttempts++;
        // If BLOCKING_THREASHOLD number of pull requests have been made whilst waiting for a
        // downstream component to respond with a pull - assume death, remove channel and continue.
        if(numberAttempts > CONFIG_BLOCKING_THRESHOLD){   
            numberChannels--;
            numberAttempts = 0;
            if(numberChannels < 1){
                Event e(id, SPLITTER_DEACTIVATE_CHANNEL);
            }
        }
    }
    return DEVICE_OK;
}

/**
 * Provide the next available ManagedBuffer to our downstream caller, if available.
 */
ManagedBuffer StreamSplitter::pull()
{
    processed++;
    return lastBuffer;
}

/**
 * Called by downstream components to register this splitter as its dataSource.
 */
void StreamSplitter::connect(DataSink &downstream)
{
    int placed = 0;
    DMESG("%s %p","Adding New Channel, ", &downstream);

    for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
    {
        // Add downstream as one of the splitters datasinks that will be served
        if (outputChannels[i] == NULL){
            outputChannels[i] = &downstream;
            placed = 1;
            DMESG("%s %d","Channel Added at location ", i);
            break;
        }
        else{
            DMESG("Channel Filled, Trying Next one");
        }
    }
    if(placed == 1){
        numberChannels++;
        processed++;
        
            for (int i = 0; i < CONFIG_MAX_CHANNELS; i++)
            {
                DMESG("%p", outputChannels[i]);
                DMESG("%s", "-------");
            }
    }
    else{
        DMESG("Channel Not Added - Max Number of Channels Reached?");
    }

    if(numberChannels > 0){
        //Activate ADC
        Event e(id, SPLITTER_ACTIVATE_CHANNEL);
    }
}

/**
 *  Determine the data format of the buffers streamed out of this component.
 */
int StreamSplitter::getFormat()
{
    return upstream.getFormat();
}

/**
 * Defines the data format of the buffers streamed out of this component.
 * @param format the format to use
 */
int StreamSplitter::setFormat(int format)
{
    return DEVICE_NOT_SUPPORTED;
}

/**
 * Destructor.
 */
StreamSplitter::~StreamSplitter()
{
}
