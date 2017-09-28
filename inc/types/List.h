#ifndef CODAL_LIST_H
#define CODAL_LIST_H

#include "ManagedBuffer.h"

namespace codal
{
    struct ListItem
    {
        ManagedBuffer b;
        ListItem* next;

        ListItem(ManagedBuffer b, ListItem* next)
        {
            this->b = b;
            this->next = next;
        }

        ListItem()
        {
            this->b = ManagedBuffer();
            this->next = NULL;
        }
    };

    typedef enum ListComparisonValue
    {
        lessThan = -1,
        equal,
        greaterThan
    } ListComparisonValue;


    class ListComparator
    {
    public:

        virtual ListComparisonValue compare(ManagedBuffer, ManagedBuffer)
        {
            return ListComparisonValue::lessThan;
        }
    };

    class List
    {
        ListItem head;
        int len;

        ListComparator& comparator;

    public:

        List();

        List(ListComparator& c);

        int insert(uint8_t* data, uint32_t len);

        ManagedBuffer remove(uint32_t index);

        ManagedBuffer remove(uint8_t* data, uint32_t len);

        ManagedBuffer get(uint32_t index);

        int length();
    };
}

#endif