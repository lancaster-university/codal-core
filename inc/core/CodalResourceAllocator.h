#ifndef CODAL_RESOURCE_ALLOCATOR
#define CODAL_RESOURCE_ALLOCATOR

// it is typical of this peripheral to be subdivided (i.e. it has multiple "channels")
#define PERIPHERAL_ATTR_SHARED                      0x01
// this peripheral is dma capable
#define PERIPHERAL_ATTR_DMA_CAPABLE                 0x02
// this peripheral is capable of commanding devices (e.g. I2C/SPI controller)
#define PERIPHERAL_ATTR_CONTROLLER_CAPABLE          0x04
// this peripheral is capable of being commanded by other devices (e.g. I2C/SPI device)
#define PERIPHERAL_ATTR_SUBORDINATE_CAPABLE         0x08

enum PeripheralType {
    PERIPHERAL_TYPE_I2C = 0x01,
    PERIPHERAL_TYPE_SPI = 0x02,
    PERIPHERAL_TYPE_PPI = 0x04,
    PERIPHERAL_TYPE_PWM = 0x08,
    PERIPHERAL_TYPE_UART = 0x10,
    PERIPHERAL_TYPE_I2C = 0x020,
}

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
         * @param driver a pointer to the driver instance that intends to command the peripheral
         * 
         * @return DEVICE_OK on success or DEVICE_NO_RESOURCES if already allocated.
         **/
        int allocatePeripheral(void* peripheral, void* driver);

        /**
         * Records a subset of the given peripheral as allocated.
         * 
         * Some peripherals have many channels. This API allows software drivers
         * to mark a subset of a peripheral as in use.
         * 
         * @param peripheral a pointer to the required peripheral
         * @param driver a pointer to the driver instance that intends to command the peripheral
         * @param rangeStart channel index range start
         * @param peripheral channel index range end
         * 
         * @return DEVICE_OK success or DEVICE_NO_RESOURCES if there is a collision.
         **/
        int allocateSharedPeripheral(void* peripheral, void* driver, int rangeStart, int rangeEnd);

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
        int freeSharedPeripheral(void* peripheral, void* driver);

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
        
        /**
         * Retrieves a pointer to the CODAL driver commanding the given peripheral.
         * 
         * @param peripheral a pointer to the required peripheral
         * 
         * @return a pointer to the driver in control of the given peripheral. NULL if
         *         none is found.
         **/
        void* obtainDriver(void* peripheral);

    };
}

#endif