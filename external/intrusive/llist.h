#pragma once

/* 
    a cute intrusive list

    Copyright Maxwell Orth 2025
*/

#include "nest_mem.h"

namespace intrusive
{
    template <typename T>
    struct list_inject
    {
        T* left = nullptr;
        T* right = nullptr;
    };

    //template <typename T, list_inject<T> T::*inject>
    template <typename T, typename MemberChain>
        requires member_chain_resolves<list_inject<T>*, T, MemberChain>
    struct list
    {

        T* head = nullptr;
        T* tail = nullptr;

        static list_inject<T>* resolve(T* t)
        {
            return MemberChain::resolve(t);
            //return &(t->*inject);
        }

        void push_front(T* t)
        {
            if (head == nullptr)
            {
                head = t;
                tail = head;
                list_inject<T>* info = resolve(t);
                info->left = nullptr;
                info->right = nullptr;
            }
            else
            {
                insert_before(head, t);
            }
        }

        void push_back(T* t)
        {
            if (head == nullptr)
            {
                head = t;
                tail = head;
                list_inject<T>* info = resolve(t);
                info->left = nullptr;
                info->right = nullptr;
            }
            else
            {
                insert_after(tail, t);
            }
        }

        void insert_after(T* it, T* element)
        {
            list_inject<T>* left_info = resolve(it);
            list_inject<T>* e_info = resolve(element);
            e_info->left = it;
            e_info->right = left_info->right;
            if (left_info->right == nullptr)
            {
                tail = element;
            }
            else
            {
                list_inject<T>* right_info = resolve(left_info->right);
                right_info->left = element;
            }
            left_info->right = element;
        }

        void insert_before(T* it, T* element)
        {
            list_inject<T>* right_info = resolve(it);
            list_inject<T>* e_info = resolve(element);
            e_info->left = right_info->left;
            e_info->right = it;

            if (right_info->left == nullptr)
            {
                head = element;
            }
            else
            {
                list_inject<T>* left_info = resolve(right_info->left);
                left_info->right = element;
            }
            right_info->left = element;
        }

        void remove(T* t)
        {
            list_inject<T>* info = resolve(t);
            if (info->left != nullptr)
            {
                list_inject<T>* l_info = resolve(info->left);
                l_info->right = info->right;
            }
            else
            {
                head = info->right;
            }

            if (info->right != nullptr)
            {
                list_inject<T>* r_info = resolve(info->right);
                r_info->left = info->left;
            }
            else
            {
                tail = info->left;
            }
        }

        void unlink(T* t)
        {
            remove(t);
        }
    };
}





