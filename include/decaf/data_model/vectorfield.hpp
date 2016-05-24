#ifndef VECTORFIELD_HPP
#define VECTORFIELD_HPP

#include <decaf/data_model/basefield.hpp>
#include <decaf/data_model/vectorconstructdata.hpp>
#include <memory>


namespace decaf {


template <typename T>
class VectorField : public BaseField {

public:
    VectorField(std::shared_ptr<BaseConstructData> ptr)
    {
        ptr_ = std::dynamic_pointer_cast<VectorConstructData<T> >(ptr);
        if(!ptr_)
            std::cerr<<"ERROR : Unable to cast pointer to VectorConstructData<T> when using a VectorField."<<std::endl;
    }

    VectorField(mapConstruct map = mapConstruct())
    {
        ptr_ = std::make_shared<VectorConstructData<T> >(map);
    }

    VectorField(std::vector<T>& value, int element_per_items, mapConstruct map = mapConstruct())
    {
        ptr_ = std::make_shared<VectorConstructData<T> >(value, element_per_items, map);
    }

    VectorField(typename std::vector<T>::iterator begin, typename std::vector<T>::iterator end,
                int element_per_items, mapConstruct map = mapConstruct())
    {
        ptr_ = std::make_shared<VectorConstructData<T> >(begin, end, element_per_items, map);
    }

    VectorField(T* Vector, int size, int element_per_items, mapConstruct map = mapConstruct())
    {
        ptr_ = std::make_shared<VectorConstructData<T> >(Vector, size, element_per_items, map);
    }

    virtual BaseConstructData* operator -> () const
    {
        return ptr_.get();
    }

    virtual std::shared_ptr<BaseConstructData> getBasePtr()
    {
        return ptr_;
    }

    std::shared_ptr<VectorConstructData<T> > getPtr()
    {
        return ptr_;
    }

    bool empty()
    {
        return ptr_.use_count() == 0;
    }

    std::vector<T>& getVector()
    {
        return ptr_->getVector();
    }

    int getNbItems()
    {
        return ptr_->getNbItems();
    }

    virtual operator bool() const {
          return ptr_ ? true : false;
        }

private:
    std::shared_ptr<VectorConstructData<T> > ptr_;
};

typedef VectorField<int> VectorFieldi;
typedef VectorField<unsigned int> VectorFieldu;
typedef VectorField<float> VectorFieldf;
typedef VectorField<double> VectorFliedd;

} // namespace
#endif
