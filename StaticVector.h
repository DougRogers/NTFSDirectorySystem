#pragma once
#include <assert.h>


template <size_t VECTOR_BUCKET_SIZE, class _Type> class StaticVector
{
    typedef std::vector<_Type> VectorOfType;

    std::vector<std::shared_ptr<VectorOfType>> m_data;

    size_t allocated_size;
    size_t remainder;

public:
    class iterator
    {
    public:
        int i;
        StaticVector *sv;

        iterator()
        {
            i = 0;
            sv = 0;
        }

        iterator &operator=(const iterator &other)
        {
            i = other.i;
            sv = other.sv;
            return *this;
        }

        _Type *operator->() const
        {

            return &(*sv)[i];
        }

        _Type *operator->()
        {

            return &(*sv)[i];
        }

        // prefix
        iterator operator++()
        {
            ++i;
            return *this;
        }
        // postfix
        iterator operator++(int)
        {
            i++;
            return *this;
        }
    };

    iterator begin()
    {
        iterator it;
        it.sv = this;
        it.i = 0;
        return it;
    }

    iterator end()
    {
        iterator it;
        it.sv = this;
        it.i = 0;
        return it;
    }

    StaticVector &operator=(const StaticVector &v)
    {
        resize(v.size()); // allocates
        for (int i = 0; i < v.m_data.size(); i++)
        {
            *m_data[i] = *v.m_data[i];
        }
        allocated_size = v.allocated_size;
        remainder = v.remainder;
        return *this;
    }
    StaticVector()
    {
        allocated_size = 0;
        remainder = 0;
    }

    void push_back(const _Type &data)
    {
        size_t currentSize = size();

        resize(currentSize + 1);
        (*this)[currentSize] = data;
    }

    _Type &back()
    {
        assert(size() > 0);
        return (*this)[size() - 1];
    }

    void resize(size_t newSize)
    {
        if (newSize == 0)
        {
            clear();
            return;
        }
        else if (newSize == allocated_size)
            return;
        else if (newSize > allocated_size)
        {
            size_t bigBuckets = (newSize / VECTOR_BUCKET_SIZE) + 1;

            while (bigBuckets > m_data.size())
            {
                // VectorOfType * pVector = new VectorOfType(VECTOR_BUCKET_SIZE);

                std::shared_ptr<VectorOfType> vp(new VectorOfType(VECTOR_BUCKET_SIZE));
                m_data.push_back(vp);
            }

            // usage in last vector
            remainder = newSize - (bigBuckets - 1) * VECTOR_BUCKET_SIZE;

            allocated_size = newSize;
        }
        else if (newSize < allocated_size)
        {
            // don't delete on shrink
            size_t bigBuckets = (newSize / VECTOR_BUCKET_SIZE) + 1;
            allocated_size = newSize;
            remainder = newSize - (bigBuckets - 1) * VECTOR_BUCKET_SIZE;
        }
    }
    // indexing

    void clear()
    {
        /*for(int i = 0; i < m_data.size(); i++)
        {
        delete m_data[i];
        } */
        m_data.clear();
        allocated_size = 0;
        remainder = 0;
    }

    size_t size() const
    {
        return allocated_size;
    }

    _Type &operator[](size_t i)
    {

#ifdef _DEBUG
        assert(i < allocated_size);
#endif

        size_t bucket = i / VECTOR_BUCKET_SIZE;
        size_t rem = i - bucket * VECTOR_BUCKET_SIZE;

        VectorOfType &vectorReference = *m_data[bucket];
        return vectorReference[rem];
    };

    const _Type &operator[](size_t i) const
    {
#ifdef _DEBUG
        assert(i < allocated_size);
#endif
        size_t bucket = i / VECTOR_BUCKET_SIZE;
        size_t rem = i - bucket * VECTOR_BUCKET_SIZE;

        VectorOfType &vectorReference = *m_data[bucket];
        return vectorReference[rem];
    }

    ~StaticVector()
    {
        clear();
    }
};
