
#pragma once

#include "nest_mem.h"
#include <cmath>
#include <bit>
#include <assert.h>

namespace intrusive
{

    template <typename T>
    struct heap_inject
    {
        mutable T* parent = nullptr;
        mutable T* c0 = nullptr;
        mutable T* c1 = nullptr;
    };

    template <typename T, typename MemberChain>
        requires member_chain_resolves<heap_inject<T>*, T, MemberChain>
    struct heap
    {
    #ifndef INTRUSIVE_EXPOSE_INTERNALS
    private:
    #endif
        T* root = nullptr;
        uint32_t num = 0;

        static heap_inject<T> * resolve(T * t)
        {
            return MemberChain::resolve(t);
        }

        T** getLastSlot(T*& outParent)
        {
            T* parent = nullptr;
            T** curr = &root;
            uint32_t c = std::bit_width(num+1)-1;
            for (unsigned i = c; i > 0; --i)
            {
                parent = *curr;
                if ((num+1) & (1u<<(i-1)))
                { 
                    curr = &resolve(*curr)->c0;
                }
                else
                {
                    curr = &resolve(*curr)->c1;
                }
            }
            outParent = parent;
            return curr;
        }

        T* getLast()
        {
            T* curr = root;
            uint32_t c = std::bit_width(num)-1;
            for (unsigned i = c; i > 0; --i)
            {
                if ((num) & (1u<<(i-1)))
                {
                    curr = resolve(curr)->c0;
                }
                else
                {
                    curr = resolve(curr)->c1;
                }
            }
            return curr;
        }

        void swap(T* a, T* b)
        {
            auto* ad = resolve(a);
            auto* bd = resolve(b);

            T* a0 = bd->c0;
            T* a1 = bd->c1;
            T* b0 = ad->c0;
            T* b1 = ad->c1;

            if (a0 == a)
            {
                a0 = b;
            }
            else if (a1 == a)
            {
                a1 = b;
            }
            
            if (b0 == b)
            {
                b0 = a;
            }
            else if (b1 == b)
            {
                b1 = a;
            }

            T* ap = ad->parent;
            T* bp = bd->parent;
            if (ap)
            {
                if (ap != b)
                {
                    if (resolve(ap)->c0 == a)
                    {
                        resolve(ap)->c0 = b;
                    }
                    else if (resolve(ap)->c1 == a)
                    {
                        resolve(ap)->c1 = b;
                    }
                    else
                    {
                        assert(("not a child of parent", false));
                    }
                }
            }
            else
            {
                assert(root == a);
                root = b;
            }
            if (bp)
            {
                if (bp != a)
                {
                    if (resolve(bp)->c0 == b)
                    {
                        resolve(bp)->c0 = a;
                    }
                    else if (resolve(bp)->c1 == b)
                    {
                        resolve(bp)->c1 = a;
                    }
                    else
                    {
                        assert(("not a child of parent", false));
                    }
                }
            }
            else
            {
                assert(root == b);
                root = a;
            }
            ad->parent = bp;
            bd->parent = ap;

            if (a0) { resolve(a0)->parent = a; }
            if (a1) { resolve(a1)->parent = a; }
            if (b0) { resolve(b0)->parent = b; }
            if (b1) { resolve(b1)->parent = b; }

            ad->c0 = a0;
            ad->c1 = a1;
            bd->c0 = b0;
            bd->c1 = b1;
        }

    public:

        void insert(T* t)
        {
            T* parent = nullptr;
            T** curr = nullptr;
            if (root == nullptr)
            {
                ++num;
                root = t;
                return;
            }
            
            curr = getLastSlot(parent);
            *curr = t;
            resolve(t)->parent = parent;

            uint32_t f = 1;
            while (parent)
            {
                if (*t < *parent)
                {
                    swap(t, parent);
                    parent = resolve(t)->parent;
                }
                else
                {
                    break;
                }
            }
            ++num;
        }

        T* pop()
        {
            T* r = root;

            if (r)
            {
                remove(r);
            }

            return r;
        }

        T * peek()
        {
            return root;
        }

        void remove(T* t)
        {
            if (t == root && num == 1)
            {
                root = nullptr;
                --num;
                return;
            }

            T* last = getLast();
            // swap with last, unlink t, then sift last

            auto* ld = resolve(last);

            assert(ld->c0 == nullptr);
            assert(ld->c1 == nullptr);
                
            swap(t, last); 

            if (resolve(t)->parent)
            {
                auto* tp = resolve(resolve(t)->parent);
                if (tp->c0 == t)
                {
                    tp->c0 = nullptr;
                }
                else if (tp->c1 == t)
                {
                    tp->c1 = nullptr;
                }
            }
            resolve(t)->parent = nullptr;

            assert(resolve(t)->c0 == nullptr);
            assert(resolve(t)->c1 == nullptr);

            T* parent = ld->parent;


            int dir = 0;
            while (true)
            {
                if (parent && *last < *parent)
                {
                    if (dir == 0)
                    {
                        dir = 1;
                    }
                    assert(dir == 1);
                    // swap with parent
                    swap(last, parent);
                    parent = resolve(last)->parent;
                }
                else
                {
                    if (dir == 0)
                    {
                        dir = -1;
                    }
                    assert(dir == -1);
                    bool c0s = ld->c0 ? (*ld->c0 < *last) : false;
                    bool c1s = ld->c1 ? (*ld->c1 < *last) : false;
                    if (c0s && c1s)
                    {
                        if (*ld->c0 < *ld->c1)
                        {
                            swap(last, ld->c0);
                        }
                        else
                        {
                            swap(last, ld->c1);
                        }
                    }
                    else if (c0s)
                    {
                        swap(last, ld->c0);
                    }
                    else if (c1s)
                    {
                        swap(last, ld->c1);
                    }
                    else
                    {
                        break;
                    }
                }
            }
            --num;
        }

    };



}
