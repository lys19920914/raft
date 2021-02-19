#pragma once

#include <msgpack.hpp>
#include <string>
#include <iostream>
#include <tuple>
#include <sstream>
#include <functional>
#include <utility>
#include <type_traits>

//#include "test.pb.h"
template<typename T>
void LOG(T Msg) {
    std::cerr << Msg << std::endl;
}

template<typename First, typename... Last>
void LOG(First Head, Last... Others) {
    std::cerr << Head;
    LOG(Others...);
}

template<typename T>
struct ShowType;
static const uint16_t CALLBACK_HANDLE = 0;

template<typename ConnType>
class RPC {
    using ListType = std::vector<std::shared_ptr<msgpack::object_handle>>;

    struct TFuncType//instead std:function<>, enable move semantic, not type safe
    {
        struct ImplBase {
            ImplBase() = default;

            ImplBase(const ImplBase &) = delete;

            virtual void Call(ConnType *, const ListType &) = 0;

            virtual ~ImplBase() {};
        };

        template<typename FTYPE>
        struct ImplType : ImplBase {
            FTYPE RealFunc;

            template<typename T>
            ImplType(T &&Fn):RealFunc{std::forward<T>(Fn)} {}

            //ImplType(ImplType&& ToMove):RealFunc(std::move(ToMove.RealFunc)){}
            void Call(ConnType *ConnPtr, const ListType &ArgList) override {
                //ShowType<decltype(RealFunc)> Show;
                RealFunc(ConnPtr, ArgList);
            }
        };

        ImplBase *ImplPtr = nullptr;

        template<typename F>
        TFuncType(F &&Fn):ImplPtr{new ImplType<F>(std::forward<F>(Fn))} {}

        TFuncType(const TFuncType &) = delete;

        TFuncType(TFuncType &&Other) {
            ImplPtr = Other.ImplPtr;
            Other.ImplPtr = nullptr;
        }

        void operator()(ConnType *ConnPtr, const ListType &ArgList) {
            assert(nullptr != ImplPtr);
            ImplPtr->Call(ConnPtr, ArgList);
        }

        ~TFuncType() {
            delete ImplPtr;
            ImplPtr = nullptr;
        }
    };

    std::unordered_map<uint16_t, TFuncType> CmdMap;
    uint16_t NextSessionID = 0;
    std::vector<uint16_t> UnusedSId;
    std::unordered_map<uint16_t, std::tuple<int, TFuncType>> CallBackMap;

    template<typename...> using my_void_t = void;//std::void_t for C++14
    template<typename T, typename = void>
    struct function_traits;
    template<typename T>
    struct function_traits<T, my_void_t<decltype(&T::operator())>> : function_traits<decltype(&T::operator())> {
        using ClassType = void;
    };
    template<typename R, typename... Args>
    struct function_traits<R(*)(ConnType *, Args...)> {
        using ArgsType = std::tuple<Args...>;
        using RetType = R;
        using ClassType = void;
    };
    template<typename R, typename... Args>
    struct function_traits<R(ConnType::*)(Args...)> {
        using ArgsType = std::tuple<Args...>;
        using RetType = R;
        using ClassType = ConnType;
    };
    template<typename R, typename... Args>
    struct function_traits<R(ConnType::*)(Args...) const> {
        using ArgsType = std::tuple<Args...>;
        using RetType = R;
        using ClassType = ConnType;
    };
    template<typename R, typename CLambda, typename... Args>
    struct function_traits<R(CLambda::*)(ConnType *, Args...)> {
        using ArgsType = std::tuple<Args...>;
        using RetType = R;
        using ClassType = CLambda;
    };
    template<typename R, typename CLambda, typename... Args>
    struct function_traits<R(CLambda::*)(ConnType *, Args...) const> {
        using ArgsType = std::tuple<Args...>;
        using RetType = R;
        using ClassType = CLambda;
    };

    template<bool, typename Type>
    struct Convert {
        Type operator()(const ListType &List, size_t Index) {
            Type Ret;
            return List[Index]->get().convert(Ret);
        }
    };
    //template<typename Type> struct Convert<true, Type>
    //{
    //Type operator () (const ListType& List, size_t Index)
    //{
    //Type Ret;
    //std::string BinData;
    //List[Index]->get().convert(BinData);
    //Ret.ParseFromString(BinData);
    //return Ret;
    //}
    //};

    template<typename Type>
    static auto ConvertByIndex(const ListType &List, size_t Index) -> typename std::decay<Type>::type {
        //return Convert<std::is_base_of<google::protobuf::Message, Type>::value, Type>{}(List, Index);
        return Convert<false, typename std::decay<Type>::type>{}(List, Index);
    }

    template<typename, typename>
    struct CallFunction;

    template<typename TupleType>
    struct CallFunction<void, TupleType> {
        template<typename FTYPE, size_t... Index>
        void
        operator()(ConnType *ConnPtr, const FTYPE &Func, const ListType &ObjectList, std::index_sequence<Index...>) {
            Func(ConnPtr, ConvertByIndex<typename std::tuple_element<Index, TupleType>::type>(ObjectList, Index)...);
        }
    };

    template<typename TupleType>
    struct CallFunction<ConnType, TupleType> {
        template<typename FTYPE, size_t... Index>
        void
        operator()(ConnType *ConnPtr, const FTYPE &Func, const ListType &ObjectList, std::index_sequence<Index...>) {
            (ConnPtr->*Func)(ConvertByIndex<typename std::tuple_element<Index, TupleType>::type>(ObjectList, Index)...);
        }
    };
    //template<typename FType, typename TupleType, size_t... Index> void CallFunction(ConnType* ObjPtr, const FType& Func, const ListType& ObjectList, std::index_sequence<Index...>)
    //{
    //Func(ObjPtr, ConvertByIndex<typename std::tuple_element<Index, TupleType>::type>(ObjectList, Index)...);
    //}

    //template<typename R, typename TupleType, size_t... Index> void CallFunction(ConnType* ObjPtr, R (ConnType::*Func)(typename std::tuple_element<Index, TupleType>::type...), const ListType& ObjectList, std::index_sequence<Index...>)
    //{
    //(ObjPtr->*Func)(ConvertByIndex<typename std::tuple_element<Index, TupleType>::type>(ObjectList, Index)...);
    //}
    template<bool, typename Type>
    struct PackOne {
        void operator()(msgpack::sbuffer &ss, const Type &Value) {
            msgpack::pack(ss, Value);
        }
    };

    template<typename Type>
    struct PackOne<true, Type> {
        void operator()(msgpack::sbuffer &ss, const Type &Value) {
            std::string Str;
            if (Value.SerializeToString(&Str)) {
                msgpack::pack(ss, Str);
            }
        }
    };

    void PackArgs(msgpack::sbuffer &ss) {}

    template<typename Type>
    void PackArgs(msgpack::sbuffer &ss, Type &&Arg) {
        //PackOne<std::is_base_of<google::protobuf::Message, Type>::value, Type>()(ss, Arg);
        PackOne<false, Type>()(ss, std::forward<Type>(Arg));
    }

    template<typename Type, typename... Args>
    void PackArgs(msgpack::sbuffer &ss, Type &&First, Args &&... Last) {
        //PackOne<std::is_base_of<google::protobuf::Message, Type>::value, Type>()(ss, First);
        PackOne<false, Type>()(ss, std::forward<Type>(First));
        PackArgs(ss, std::forward<Args>(Last)...);
    }

    template<typename FTYPE>
    auto WrapRegister(FTYPE &&Function) {
        typedef typename function_traits<std::decay_t<FTYPE>>::ArgsType ArgsType;
        typedef typename function_traits<std::decay_t<FTYPE>>::ClassType ClassType;
        return [Func = std::forward<FTYPE>(Function)](ConnType *ConnPtr, const ListType &ArgsList) {
            constexpr size_t ArgSize = std::tuple_size<ArgsType>::value;
            CallFunction<ClassType, ArgsType>()(ConnPtr, Func, ArgsList, std::make_index_sequence<ArgSize>{});
        };
    }

    static bool OnCallBack(RPC<ConnType> *RPCPtr, ConnType *ConnPtr, const ListType &ResultsList) {
        //session at last
        if (ResultsList.empty()) {
            return false;
        }
        uint16_t SId = 0;
        ResultsList.back()->get().convert(SId);
        //ResultsList.pop_back();
        RPCPtr->UnusedSId.push_back(SId);
        auto It = RPCPtr->CallBackMap.find(SId);
        if (It != RPCPtr->CallBackMap.end()) {
            std::get<1>(It->second)(ConnPtr, ResultsList);
            RPCPtr->CallBackMap.erase(It);
            return true;
        }
        return false;
    }

public:
    RPC() {
        //RegisterCmd(0, &RPC<ConnType>::OnCallBack); cann't do this, wrap with a lambda
        //RegisterCmd(0, [this](ConnType* ConnPtr, ListType&& ResultsList)
        CmdMap.emplace(CALLBACK_HANDLE,//hardcode
                       [this](ConnType *ConnPtr, const ListType &ResultsList) {
                           OnCallBack(this, ConnPtr, std::move(ResultsList));
                       }
        );
    }

    template<typename FTYPE>
    uint16_t RegisterCallback(FTYPE &&Function) {
        uint16_t SessionId = 0;
        if (UnusedSId.empty()) {
            SessionId = ++NextSessionID;
        } else {
            SessionId = UnusedSId.back();
            UnusedSId.pop_back();
        }
        CallBackMap.emplace(SessionId, std::make_tuple(0, WrapRegister(std::forward<FTYPE>(Function))));
        return SessionId;
    }

    template<typename FTYPE>
    bool RegisterCmd(uint16_t CmdId, FTYPE &&Function) {
        if (CmdId < 10) {
            LOG(__FILE__, ":", __LINE__, "RegisterCmd ERROR, CmdId must >= 10, ", CmdId);
            return false;
        }
        CmdMap.emplace(CmdId, WrapRegister(std::forward<FTYPE>(Function)));
        return true;
    }

    bool UnpackCmd(ConnType *ObjPtr, const char *DataPtr, size_t Len) {
        msgpack::unpacker UPacker;
        UPacker.reserve_buffer(Len);
        memcpy(UPacker.buffer(), DataPtr, Len);
        UPacker.buffer_consumed(Len);
        uint16_t CmdId = 0;
        msgpack::object_handle oh;
        if (UPacker.next(oh)) {
            CmdId = oh.get().as<uint16_t>();
        } else {
            return false;
        }
        ListType List;
        while (UPacker.next(oh)) {
            List.emplace_back(std::make_shared<msgpack::object_handle>(std::move(oh)));
        }
        auto It = CmdMap.find(CmdId);
        if (It != CmdMap.end()) {
            try {
                It->second(ObjPtr, std::move(List));
            } catch (const msgpack::type_error &e) {
                LOG(__FILE__, ":", __LINE__, "type_error for cmd ", CmdId, e.what());
            }
        }
        return false;
    }

    bool UnpackCmd(ConnType *ObjPtr, unsigned short SId, const char *DataPtr, size_t Len) {
        msgpack::unpacker UPacker;
        UPacker.reserve_buffer(Len);
        memcpy(UPacker.buffer(), DataPtr, Len);
        UPacker.buffer_consumed(Len);
        uint16_t CmdId = 0;
        msgpack::object_handle oh;
        if (UPacker.next(oh)) {
            CmdId = oh.get().as<uint16_t>();
        } else {
            return false;
        }
        ListType List;

        msgpack::sbuffer ss;
        msgpack::pack(ss, SId);
        msgpack::object_handle oh_SId = msgpack::unpack(ss.data(), ss.size());
        List.emplace_back(std::make_shared<msgpack::object_handle>(std::move(oh_SId)));

        while (UPacker.next(oh)) {
            List.emplace_back(std::make_shared<msgpack::object_handle>(std::move(oh)));
        }
        auto It = CmdMap.find(CmdId);
        if (It != CmdMap.end()) {
            try {
                It->second(ObjPtr, std::move(List));
            } catch (const msgpack::type_error &e) {
                LOG(__FILE__, ":", __LINE__, "type_error for cmd ", CmdId, e.what());
            }
        }
        return false;
    }

    template<typename... Args>
    msgpack::sbuffer PackCmd(uint16_t FId, Args &&... AllArgs) {
        msgpack::sbuffer ss;
        msgpack::pack(ss, FId);
        PackArgs(ss, std::forward<Args>(AllArgs)...);
        return ss;
    }

    template<typename... Args>
    msgpack::sbuffer PackCmdWithHead(uint16_t FId, Args &&... AllArgs) {
        msgpack::sbuffer ss;
        char Head[2] = {0, 0};
        ss.write(Head, sizeof(Head));
        msgpack::pack(ss, FId);
        PackArgs(ss, std::forward<Args>(AllArgs)...);
        *(uint16_t *) ss.data() = ss.size() - sizeof(Head);
        return ss;
    }

    template<typename... Args>
    msgpack::sbuffer PackResponse(uint16_t SId, Args &&... AllArgs) {
        msgpack::sbuffer ss;
        msgpack::pack(ss, CALLBACK_HANDLE);
        PackArgs(ss, std::forward<Args>(AllArgs)...);
        msgpack::pack(ss, SId);
        return ss;
    }

    template<typename... Args>
    msgpack::sbuffer PackResponseWithHead(uint16_t SId, Args &&... AllArgs) {
        msgpack::sbuffer ss;
        char Head[2] = {0, 0};
        ss.write(Head, sizeof(Head));
        msgpack::pack(ss, CALLBACK_HANDLE);
        PackArgs(ss, std::forward<Args>(AllArgs)...);
        msgpack::pack(ss, SId);
        *(uint16_t *) ss.data() = ss.size() - sizeof(Head);
        return ss;
    }
};

