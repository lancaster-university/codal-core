#ifndef CODAL_RESOURCE_ALLOCATOR
#define CODAL_RESOURCE_ALLOCATOR

/**
  * Class definition for the CodalResourceAllocator.
  *
  * The CodalResourceAllocator is the centralised abstraction for software drivers to request access to resources.
  *
  */
namespace codal
{
    typedef void (*CodalResourceAllocatorCB)(void *);

    class CodalResourceAllocator
    {
        public:

        /**
          * The default constructor of a CodalResourceAllocator
          */
        CodalResourceAllocator()
        {
        }

        /**
         * Records the given peripheral as allocated, if not already.
         * 
         * @param peripheral a pointer to the required peripheral
         * 
         * @return DEVICE_OK on success or DEVICE_NO_RESOURCES if already allocated.
         **/
        int allocatePeripheral(void* peripheral);

        /**
         * Records a subset of the given peripheral as allocated.
         * 
         * Some peripherals have many channels. This API allows software drivers
         * to mark a subset of a peripheral as in use.
         * 
         * @param peripheral a pointer to the required peripheral
         * @param rangeStart channel index range start
         * @param peripheral channel index range end
         * 
         * @return handle on success or DEVICE_NO_RESOURCES if there is a collision.
         **/
        int allocateSharedPeripheral(void* peripheral, int rangeStart, int rangeEnd);

        /**
         * Records the given peripheral as free for other software drivers to use.
         * 
         * @param peripheral a pointer to the required peripheral
         * 
         * @return DEVICE_OK on success or DEVICE_NO_RESOURCES if already allocated.
         **/
        int freePeripheral(void* peripheral);

        /**
         * Records a subset of a given peripheral as free for other software drivers to use.
         * 
         * Records a subset of the given peripheral as allocated.
         * 
         * Some peripherals have many channels. This API allows software drivers
         * to mark a subset of a peripheral as in use.
         * 
         * @param peripheral a pointer to the required peripheral
         * 
         * @param handle a number returned from a previous call to allocateSharedPeripheral
         * 
         * @return DEVICE_OK on success, DEVICE_INVALID_PARAMETER if the handle is not found.
         **/
        int freeSharedPeripheral(void* peripheral, int handle);

        /**
         * Returns the IRQ number for the given peripheral.
         * 
         * @param peripheral a pointer to the required peripheral
         * 
         * @param cb a function pointer matching
         * 
         * @return IRQn on success or DEVICE_NO_RESOURCES if peripheral is not found.
         **/
        int setPeripheralIRQHandler(void* peripheral, CodalResourceAllocatorCB cb);

        /**
         * Returns the IRQ number for the given peripheral.
         * 
         * @param peripheral a pointer to the required peripheral
         * 
         * @return IRQn on success or DEVICE_NO_RESOURCES if peripheral is not found.
         **/
        int getIRQn(void* peripheral);
        
    };
}

#endif