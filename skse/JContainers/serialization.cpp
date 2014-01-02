#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/split_member.hpp>
//#include <boost/serialization/split_free.hpp>

#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_oarchive.hpp>
//#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#include <fstream>
#include <sstream>

#include "collections.h"
#include "autorelease_queue.h"
#include "shared_state.h"

#include "gtest.h"

namespace collections {



    TEST(Item, serialization)
    {
        std::ostringstream str;
        ::boost::archive::binary_oarchive arch(str);

        const char *testStr = "hey, babe!";

        auto ar = new array;
        ar->push(Item(testStr));

        {
            arch << Item(3);
            arch << Item(3.5);
            arch << Item(testStr);

            map obj;
            obj["tttt"] = Item(testStr);
            obj["array"] = Item(ar);

            arch << obj;
        }
        ar->release();

        // reading
        std::string string = str.str();
        std::istringstream istr(string);
        boost::archive::binary_iarchive ia(istr);

        Item item;
        ia >> item;
        EXPECT_TRUE(item.intValue() == 3);
        ia >> item;
        EXPECT_TRUE(item.fltValue() == 3.5);

        ia >> item;
        EXPECT_TRUE(strcmp(item.strValue(),testStr) == 0);

        map obj;
        ia >> obj;

        EXPECT_TRUE(obj.cnt.size() == 2);
        //EXPECT_TRUE(strcmp(obj["array"].strValue(), testStr) == 0);
    }

    TEST(autorelease_queue, serialization)
    {
        bshared_mutex mt;
        autorelease_queue queue(mt);
        //  queue.start();
        queue.push(10);
        queue.push(20);

        std::ostringstream str;
        ::boost::archive::binary_oarchive arch(str);
        arch << queue;

        bshared_mutex mt2;
        autorelease_queue queue2(mt2);
        // reading
        std::string string = str.str();
        std::istringstream istr(string);
        boost::archive::binary_iarchive ia(istr);

        ia >> queue2;

        EXPECT_TRUE(queue.count() == queue2.count());
        ;
    }

/*
    template<class Archive>
    inline void serialize(Archive & ar, CString & s, const unsigned int file_version) {
        split_free(ar, s, file_version); 
    }*/


    template<class Archive>
    void collection_registry::serialize(Archive & ar, const unsigned int version) {
        ar.register_type(static_cast<array *>(NULL));
        ar.register_type(static_cast<map *>(NULL));

        ar & _map;
        ar & _idGen;
    }

    template<class Archive>
    void Item::save(Archive & ar, const unsigned int version) const {
        ar.register_type(static_cast<array *>(NULL));
        ar.register_type(static_cast<map *>(NULL));

        ar & _type;
        switch (_type)
        {
        case ItemTypeNone:
            break;
        case ItemTypeInt32:
            ar & _intVal;
            break;
        case ItemTypeFloat32:
            ar & _floatVal;
            break;
        case ItemTypeCString: 
            ar & std::string(strValue() ? _stringVal->string : "");
            break;
        case ItemTypeObject:
            ar & _object;
            break;
        default:
            break;
        }
    }

    template<class Archive>
    void Item::load(Archive & ar, const unsigned int version)
    {
        ar.register_type(static_cast<array *>(NULL));
        ar.register_type(static_cast<map *>(NULL));

        ar & _type;
        switch (_type)
        {
        case ItemTypeNone:
            break;
        case ItemTypeInt32:
            ar & _intVal;
            break;
        case ItemTypeFloat32:
            ar & _floatVal;
            break;
        case ItemTypeCString:
            {
                std::string string;
                ar & string;

                if (!string.empty()) {
                    _stringVal = StringMem::allocWithString(string.c_str());
                }
                break;
            }
        case ItemTypeObject:
            ar & _object;
            break;
        default:
            break;
        }
    }

    template<class Archive>
    void object_base::serialize(Archive & ar, const unsigned int version) {
        ar & _refCount;
        ar & _type;
        ar & _id;
    }

    template<class Archive>
    void array::serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<object_base>(*this);
        ar & _array;
    }

    template<class Archive>
    void map::serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<object_base>(*this);
        ar & cnt;
    }


    void shared_state::loadAll(const vector<char> &data) {

        _DMESSAGE("%u bytes loaded", data.size());

        typedef boost::iostreams::basic_array_source<char> Device;
        boost::iostreams::stream_buffer<Device> buffer(data.data(), data.size());
        boost::archive::binary_iarchive archive(buffer);

        {
            write_lock g(_mutex);

            registry._clear();
            aqueue._clear();

            archive >> registry;
            archive >> aqueue;
            archive >> _databaseId;
        }
    }

    vector<char> shared_state::saveToArray() {
        vector<char> buffer;
        boost::iostreams::back_insert_device<decltype(buffer) > device(buffer);
        boost::iostreams::stream<decltype(device)> stream(device);
        boost::archive::binary_oarchive arch(stream);

        {
            read_lock g(_mutex);

            for (auto pair : registry._map) {
                pair.second->_mutex.lock();
            }

            arch << registry;
            arch << aqueue;
            arch << _databaseId;

            for (auto pair : registry._map) {
                pair.second->_mutex.unlock();
            }
        }

        _DMESSAGE("%u bytes saved", buffer.size());

        return buffer;
    }
}
