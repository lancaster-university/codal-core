#include "List.h"
#include "CodalConfig.h"

using namespace codal;

static ListComparator defaultComparator;

List::List() : comparator(defaultComparator)
{
    this->len = 0;
}

List::List(ListComparator& c) : comparator(c)
{
    this->len = 0;
}

int List::insert(uint8_t* data, uint32_t len)
{
    ListItem* n = new ListItem(ManagedBuffer(data, len), NULL);

    ListItem* p = &head;
    ListItem* l = head.next;

    while(l)
    {
        if (comparator.compare(n->b, l->b) == ListComparisonValue::lessThan)
            break;

        p = l;
        l = l->next;
    }

    // we could break early, if the comparator indicates so.
    if (p->next)
        n->next = p->next;

    p->next = n;


    this->len++;

    return this->len - 1;
}

ManagedBuffer List::get(uint32_t index)
{
    if (head.next == NULL || index > this->length() - 1)
        return ManagedBuffer();

    uint32_t i = 0;

    ListItem* l = head.next;

    while(i != index && l != NULL)
    {
        l = l->next;
        i++;
    }

    return l->b;
}

ManagedBuffer List::remove(uint32_t index)
{
    if (head.next == NULL  || index > this->length() - 1)
        return ManagedBuffer();

    uint32_t i = 0;

    ListItem* l = head.next;
    ListItem* p = &head;

    while(i != index)
    {
        p = l;
        l = l->next;
        i++;
    }

    p->next = l->next;

    ManagedBuffer ret = l->b;

    free(l);

    this->len--;

    return ret;
}

ManagedBuffer List::remove(uint8_t* data, uint32_t len)
{
    ManagedBuffer item = ManagedBuffer(data, len);

    ListItem* l = head.next;
    ListItem* p = &head;

    while(l->next && comparator.compare(item, l->b) != ListComparisonValue::equal)
        l = l->next;

    p->next = l->next;
}

int List::length()
{
    return this->len;
}