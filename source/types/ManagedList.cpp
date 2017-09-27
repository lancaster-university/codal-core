#include "ManagedList.h"

const ListItem emptyListItem(ManagedBuffer(), ListItem());

ManagedList::ManagedList()
{
    this->head = emptyListItem;
    this->len = 0;
}

ListItem* initListItem(uint8_t* data, uint32_t len, ListItem* next)
{
    ListItem* ptr = (ListItem*)malloc(sizeof(ListItem));

    REF_COUNTED_INIT(ptr);
    ptr->b = ManagedBuffer(data, len);
    ptr->next = next;

    ptr->incr();

    return ptr;
}

void ManagedList::add(uint8_t* data, uint32_t len)
{
    ListItem* n = initListItem(data, len, NULL);

    ListItem* l = head;

    while(l.next != NULL)
        l = l->next

    l->next = n;

    len++;
}

ManagedBuffer ManagedList::get(uint32_t index)
{
    if (head == emptyListItem || index > this->length() - 1)
        return emptyListItem;

    uint32_t i = 0;

    ListItem* l = head;

    while(i != index)
    {
        l = l->next;
        i++;
    }

    return l;
}

ManagedBuffer ManagedList::remove(uint32_t index)
{
    if (head == emptyListItem || index > this->length() - 1)
        return emptyListItem;

    uint32_t i = 0;

    ListItem* l = head;
    ListItem* p = head;

    while(i != index)
    {
        p = l;
        l = l->next;
        i++;
    }

    p->next = l->next;

    return l->b;
}

uint32_t ManagedList::length()
{
    return this->len;
}