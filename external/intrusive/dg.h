
#pragma once

#include "llist.h"
#include "nest_mem.h"

#include <stdio.h>

/*
    a directed graph with topological sourt
*/

namespace intrusive
{
    template <typename T>
    struct dg_edge
    {
        T* from = nullptr;
        T* to = nullptr;
        intrusive::list_inject<dg_edge<T>> fromlist;
        intrusive::list_inject<dg_edge<T>> tolist;
    };

    template <typename T>
    struct dg_inject
    {
        using fromlist_chain = member_chain<member_pointer<dg_edge<T>, intrusive::list_inject<dg_edge<T>>, &dg_edge<T>::fromlist>>;
        using tolist_chain = member_chain<member_pointer<dg_edge<T>, intrusive::list_inject<dg_edge<T>>, &dg_edge<T>::tolist>>;
        intrusive::list<dg_edge<T>, fromlist_chain> from;
        intrusive::list<dg_edge<T>, tolist_chain> to;
        intrusive::list_inject<T> nodeList;
    };
    
    template <typename T, dg_inject<T> T::*inject>
    using dg_edge_list_membchain = 
        member_chain<
            member_pointer<T, dg_inject<T>, inject>,
            member_pointer<dg_inject<T>, intrusive::list_inject<T>, &dg_inject<T>::nodeList>
        >;

    template <typename T, dg_inject<T> T::*inject>
    struct directed_graph
    {
        using node_list = intrusive::list<T, dg_edge_list_membchain<T, inject>>;

        node_list nodes;


        static dg_inject<T>* resolve(T* t)
        {
            return member_chain<member_pointer<T, dg_inject<T>, inject>>::resolve(t);
            //return &(t->*inject);
        }

        // order of operations isnt super important, edges can be set up before or after the nodes are added.
        // you could add a node multiple times and it's not gonna check it, so dont do that.
        void add(T* node)
        {
            nodes.push_front(node);
        }

        void remove(T* node)
        {
            nodes.unlink(node);
            // remove edges?
            dg_inject<T>* info = resolve(node);
            {
                dg_edge<T>* c = info->from.head;
                while (c != nullptr)
                {
                    dg_edge<T>* n = c->fromlist.right;
                    info->from.unlink(c);
                    c = n;
                }
            }
            {
                dg_edge<T>* c = info->to.head;
                while (c != nullptr)
                {
                    dg_edge<T>* n = c->tolist.right;
                    info->to.unlink(c);
                    c = n;
                }
            }
        }

        // adding an edge multiple times is bad dont do that.
        void addEdge(dg_edge<T>* edge)
        {
            dg_inject<T>* from_info = resolve(edge->from);
            dg_inject<T>* to_info = resolve(edge->to);

            from_info->from.push_front(edge);
            to_info->to.push_front(edge);
        }

        void unlink(dg_edge<T>* edge)
        {
            dg_inject<T>* from_info = resolve(edge->from);
            dg_inject<T>* to_info = resolve(edge->to);

            from_info->from.unlink(edge);
            to_info->to.unlink(edge);
        }

        // dismantles the dg, except for where there are cycles. FUN!
        bool topo_sort(node_list & out)
        {
            // clear the list...    ~~~spooky
            {
                T* c = out.head;
                while (c)
                {
                    T* next = out.resolve(c)->right;
                    out.unlink(c);
                    c = next;
                }
            }

            node_list emptyNodes;
            int count = 0;
            {
                T* n = nodes.head;
                while (n != nullptr)
                {
                    ++count;
                    dg_inject<T>* info = resolve(n);
                    T* next = nodes.resolve(n)->right;
                    if (info->to.head == nullptr)
                    {
                        nodes.unlink(n);
                        emptyNodes.push_back(n);
                    }
                    n = next;
                }
            }

            while (emptyNodes.head)
            {
                T* n = emptyNodes.head;
                emptyNodes.unlink(n);
                out.push_back(n);
                --count;

                dg_edge<T>* c = resolve(n)->from.head;
                while (c)
                {
                    dg_edge<T>* next = resolve(n)->from.resolve(c)->right;
                    unlink(c);
                    if (resolve(c->to)->to.head == nullptr)
                    {
                        emptyNodes.push_back(c->to);
                    }
                    c = next;
                }
            }

            return count == 0;
        }

    };

}
