

namespace codal
{

    class ListItem : RefCounted
    {
        ManagedBuffer b;
        ListItem* next;
    }

    class ManagedList
    {
        ListItem head;
        uint32_t len;

    public:

        ManagedList();

        void add(uint8_t* data, uint32_t len);

        ListItem get(uint32_t index);

        uint32_t length();
    }
}