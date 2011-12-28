#ifndef __STD_LIST_H__
#define __STD_LIST_H__

/*
 *  Lists
 */
#define __list__

typedef struct {
    void *prev;
    void *next;
} list_node;

#define DLINKED_NAMED_LIST(name)    list_node name;        // named list
#define DLINKED_LIST                DLINKED_NAMED_LIST(link);

#define DLINKED_LIST_INIT(_prev, _next)   \
    .link = { .next = (_next), .prev = (_prev) }
#define DLINKED_NAMED_LIST_INIT(_name, _prev, _next) \
    ._name = { .next = (_next), .prev = (_prev) }

#define list_next(node) (typeof(node))(node->link.next)
#define list_prev(node) (typeof(node))(node->link.prev)

#define nlist_next(name, node) (typeof(node))(node->name.next)
#define nlist_prev(name, node) (typeof(node))(node->name.prev)

#define list_link_next(list, next_node) (list)->link.next = (next_node)
#define list_link_prev(list, prev_node) (list)->link.prev = (prev_node)

#define nlist_link_next(name, node, next_node) (node)->(nlist).next = (next_node)
#define nlist_link_prev(name, node, prev_node) (node)->(nlist).prev = (prev_node)

#define list_head(_list)     (_list)
#define list_tail(_list)     (list_next(_list))

#define list_init(_list, _node)                 \
    do {                                        \
        (_list) = (_node);                      \
        list_link_prev((_list), null);          \
        list_link_next((_list), null);          \
    } while (0)

#define list_insert(_list, new_node)            \
    do {                                        \
        list_link_prev((new_node), null);       \
        list_link_next((new_node), (_list));    \
        if ((_list) != null)                    \
            list_link_prev((_list), (new_node));\
        (_list) = (new_node);                   \
    } while (0)

#define list_insert_after(_this_node, _new_node)    \
    do {                                            \
        list_link_next((_new_node), null);          \
        list_link_prev((_new_node), (_new_node));   \
        list_link_next((_this_node), (_new_node));  \
    } while (0)

#define list_insert_between(_new_node, _node1, _node2)  \
    do {                                        \
        list_link_prev((_new_node), (_node1));      \
        list_link_next((_new_node), (_node2));      \
        list_link_next((_node1), (_new_node));      \
        list_link_prev((_node2), (_new_node));      \
    } while (0)

#define list_append(__list, _node)                  \
    do {                                            \
        if (empty_list(__list))                     \
            list_init((__list), (_node));           \
        else {                                      \
            typeof(_node) _cur = null;              \
            list_last((__list), _cur);              \
            list_insert_after(_cur, (_node));       \
        }                                           \
    } while (0)

#define list_node_release(_node)                    \
    do {                                            \
        typeof(_node) _lnext = list_next(_node);    \
        typeof(_node) _lprev = list_prev(_node);    \
        if (! empty_list(_lnext))                   \
            list_link_prev(_lnext, list_prev(_node));   \
        if (! empty_list(_lprev))                   \
            list_link_next(_lprev, _lnext);         \
    } while (0)

#define list_release(_list, _node)                  \
    do {                                            \
        typeof(_node) _lnext = list_next(_node);    \
        typeof(_node) _lprev = list_prev(_node);    \
        if (! empty_list(_lnext))                   \
            list_link_prev(_lnext, _lprev);         \
        if (! empty_list(_lprev))                   \
            list_link_next(_lprev, _lnext);         \
        if ((_node) == (_list))                     \
            _list = _lnext; /*maybe null, it's ok*/ \
    } while (0)

#define list_last(_list, _out_node)                 \
    do {                                            \
        list_foreach(_out_node, (_list))            \
            if (empty_list(list_next(_out_node)))   \
                break;                              \
    } while (0)

#define empty_list(_node)      ((_node) == null)

#define list_foreach(_v, _list) \
    for (_v = list_head(_list); !empty_list(_v); _v = list_next(_v))

#define define_list_length_for(_type)               \
static inline size_t list_of__type_length(_type *_node) {    \
    size_t len = 0;                                 \
    list_foreach(_node, _node) ++len;               \
    return len;                                     \
}


#endif //__STD_LIST_H__
