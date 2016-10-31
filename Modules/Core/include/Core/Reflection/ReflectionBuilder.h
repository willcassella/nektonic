// ReflectionBuilder.h
#pragma once

#include "ArgAny.h"
#include "Reflection.h"

namespace sge
{
	template <typename T, typename ... ConvertedArgs>
	void constructor_wrapper(tmp::list<>, void* addr, const ArgAny* /*args*/, const ConvertedArgs&... realArgs)
	{
		new (addr) T(realArgs...);
	}

	template <typename T, typename ArgT, typename ... ArgTs, typename ... ConvertedArgs>
	void constructor_wrapper(tmp::list<ArgT, ArgTs...>, void* addr, const ArgAny* args, const ConvertedArgs&... realArgs)
	{
		constructor_wrapper<T>(
			tmp::list<ArgTs...>{},
			addr,
			args + 1,
			realArgs...,
			args->get<ArgT>());
	}

	template <typename T>
	struct NativeTypeInfoBuilder
	{
		////////////////////////
		///   Constructors   ///
	public:

		NativeTypeInfoBuilder(std::string name)
			: base_type_data(std::move(name))
		{
			if (std::is_empty<T>::value)
			{
				base_type_data.size = 0;
			}
			else
			{
				base_type_data.size = sizeof(T);
			}

			base_type_data.alignment = alignof(T&);
			base_type_data.native_type_info = &typeid(T);

			this->template set_init<T>();
			this->template set_copy_init<T>();
			this->template set_move_init<T>();
			this->template set_copy_assign<T>();
			this->template set_move_assign<T>();
			this->template set_drop<T>();
		}

		///////////////////
		///   Methods   ///
	public:

		NativeTypeInfoBuilder&& flags(TypeFlags flags)
		{
			base_type_data.flags = flags;
			return std::move(*this);
		}

		template <typename BaseT>
		NativeTypeInfoBuilder&& extends()
		{
			static_assert(std::is_base_of<BaseT, T>::value, "The given type is not actually a base of this type.");
			base_type_data.base = &sge::get_type<BaseT>();
			return std::move(*this);
		}

		template <typename InterfaceT>
		NativeTypeInfoBuilder&& implements()
		{
			type_data.interfaces.insert(std::make_pair(&sge::get_interface<InterfaceT>(), &InterfaceT::template get_impl<T>()));
			return std::move(*this);
		}

		template <typename ArgT, typename ... ArgTs>
		NativeTypeInfoBuilder&& constructor()
		{
			ConstructorInfo::Data baseCData;
			baseCData.arg_types = { &sge::get_type<ArgT>(), &sge::get_type<ArgTs>()... };

			NativeConstructorInfo::Data cData;
			cData.constructor = [](void* addr, const ArgAny* args) -> void {
				sge::constructor_wrapper<T>(tmp::list<ArgT, ArgTs...>{}, addr, args);
			};

			type_data.constructors.insert(std::make_pair(sizeof...(ArgTs)+1, NativeConstructorInfo{ std::move(baseCData), std::move(cData) }));
			return std::move(*this);
		}

		template <typename ArgT, typename ... ArgTs>
		NativeTypeInfoBuilder&& constructor(T(*constructor)(ArgTs...))
		{
			ConstructorInfo::Data baseCData;
			baseCData.arg_types = { &sge::get_type<ArgT>(), &sge::get_type<ArgTs>()... };

			NativeConstructorInfo::Data cData;
			cData.constructor = [constructor](void* addr, const ArgAny* args) -> void {
				sge::constructor_wrapper<T>(tmp::list<ArgT, ArgTs...>{}, addr, args);
			};

			type_data.constructors.insert(std::make_pair(sizeof...(ArgTs)+1, NativeConstructorInfo{ std::move(baseCData), std::move(cData) }));
			return std::move(*this);
		}

		template <typename ... ArgTs>
		NativeTypeInfoBuilder&& named_constructor(const char* name)
		{
			ConstructorInfo::Data baseCData;
			baseCData.arg_types = { &sge::get_type<ArgTs>()... };

			NativeConstructorInfo::Data cData;
			cData.constructor = [](void* addr, const ArgAny* args) -> void {
				sge::constructor_wrapper<T>(tmp::list<ArgTs...>{}, addr, args);
			};

			type_data.named_constructors.insert(std::make_pair(name, NativeConstructorInfo{ std::move(baseCData), std::move(cData) }));
			return std::move(*this);
		}

		template <typename ... ArgTs>
		NativeTypeInfoBuilder&& named_constructor(const char* name, T(*constructor)(ArgTs...))
		{
			ConstructorInfo::Data baseCData;
			baseCData.arg_types = { &sge::get_type<ArgTs>()... };

			NativeConstructorInfo::Data cData;
			cData.constructor = [constructor](void* addr, const ArgAny* args) -> void {
				sge::constructor_wrapper<T>(tmp::list<ArgTs...>{}, addr, args);
			};

			type_data.named_constructors.insert(std::make_pair(name, NativeConstructorInfo{ std::move(baseCData), std::move(cData) }));
			return std::move(*this);
		}

		/* Creates a property with a generic getter and generic setter. */
		template <typename GetFn, typename SetFn>
		NativeTypeInfoBuilder&& property(
			const char* name,
			GetFn getter,
			SetFn setter,
			PropertyFlags flags = PF_NONE)
		{
			using GetFnTraits = stde::function_traits<GetFn>;
			using SetFnTraits = stde::function_traits<SetFn>;
			using PropT = std::decay_t<typename GetFnTraits::return_type>;
			using ContextT = std::remove_pointer_t<tmp::car_n<typename SetFnTraits::arg_types, 1>>;

			static_assert(std::is_same<const PropT*, tmp::car_n<typename SetFnTraits::arg_types, 2>>::value,
				"Property type differs between getter and setter");
			static_assert(std::is_same<const ContextT*, tmp::car_n<typename GetFnTraits::arg_types, 1>>::value,
				"Context type differs between getter and setter.");

			auto adaptedGetter = adapt_function_getter(getter);
			auto adaptedSetter = adapt_function_setter(setter);
			create_property<PropT>(name, adaptedGetter, adaptedSetter, flags, &sge::get_type<ContextT>());
			return std::move(*this);
		}

		/* Creates a property with a function getter and no setter. */
		template <typename GetFn>
		NativeTypeInfoBuilder&& property(
			const char* name,
			GetFn getter,
			std::nullptr_t /*setter*/,
			PropertyFlags flags = PF_NONE)
		{
			using GetFnTraits = stde::function_traits<GetFn>;
			using PropT = std::decay_t<typename GetFnTraits::return_type>;
			using ContextT = std::remove_const_t<std::remove_pointer_t<tmp::car_n<typename GetFnTraits::arg_types, 1>>>;

			auto adaptedGetter = adapt_function_getter(getter);
			create_readonly_property<PropT>(name, adaptedGetter, flags, &sge::get_type<ContextT>());
			return std::move(*this);
		}

		/* Creates a property with a method getter and setter. */
		template <typename GetRetT, typename SetRetT, typename SetArgT>
		NativeTypeInfoBuilder&& property(
			const char* name,
			GetRetT(T::*getter)()const,
			SetRetT(T::*setter)(SetArgT),
			PropertyFlags flags = PF_NONE)
		{
			using PropT = std::decay_t<GetRetT>;
			static_assert(std::is_same<PropT, std::decay_t<SetArgT>>::value, "Getter and setter use different property types.");

			auto adaptedGetter = adapt_method_getter(getter);
			auto adaptedSetter = adapt_method_setter(setter);
			create_property<PropT>(name, adaptedGetter, adaptedSetter, flags, nullptr);
			return std::move(*this);
		}

		/* Creates a property with a method getter and no setter. */
		template <typename GetRetT>
		NativeTypeInfoBuilder&& property(
			const char* name,
			GetRetT(T::*getter)()const,
			std::nullptr_t /*setter*/,
			PropertyFlags flags = PF_NONE)
		{
			using PropT = std::decay_t<GetRetT>;

			auto adaptedGetter = adapt_method_getter(getter);
			create_readonly_property<PropT>(name, adaptedGetter, flags, nullptr);
			return std::move(*this);
		}

		/* Creates a property with a method getter and setter, where the getter requires a context. */
		template <typename GetRetT, typename GetContextT, typename SetRetT, typename SetArgT>
		NativeTypeInfoBuilder&& property(
			const char* name,
			GetRetT(T::*getter)(GetContextT)const,
			SetRetT(T::*setter)(SetArgT),
			PropertyFlags flags = PF_NONE)
		{
			using PropT = std::decay_t<GetRetT>;
			using ContextT = std::decay_t<GetContextT>;
			static_assert(std::is_same<PropT, std::decay_t<SetArgT>>::value, "Getter and setter use different property types.");

			auto adaptedGetter = adapt_method_getter(getter);
			auto adaptedSetter = adapt_method_setter(setter);
			create_property<PropT>(name, getter, setter, flags, &sge::get_type<ContextT>());
			return std::move(*this);
		}

		/* Creates a property with a method getter and no setter, where the getter requires a context. */
		template <typename GetRetT, typename GetContextT>
		NativeTypeInfoBuilder&& property(
			const char* name,
			GetRetT(T::*getter)(GetContextT)const,
			std::nullptr_t /*setter*/,
			PropertyFlags flags = PF_NONE)
		{
			using PropT = std::decay_t<GetRetT>;
			using ContextT = std::decay_t<GetContextT>;

			auto adaptedGetter = adapt_method_getter(getter);
			create_readonly_property<PropT>(name, getter, flags, &sge::get_type<ContextT>());
			return std::move(*this);
		}

		/* Creates a property with a method getter and method setter, where the setter requires a context. */
		template <typename GetRetT, typename SetRetT, typename SetContextT, typename SetArgT>
		NativeTypeInfoBuilder&& property(
			const char* name,
			GetRetT(T::*getter)()const,
			SetRetT(T::*setter)(SetContextT, SetArgT),
			PropertyFlags flags = PF_NONE)
		{
			using PropT = std::decay_t<GetRetT>;
			using ContextT = std::decay_t<SetContextT>;
			static_assert(std::is_same<PropT, std::decay_t<SetArgT>>::value, "Getter and setter use different property types.");

			auto adaptedGetter = adapt_method_getter(getter);
			auto adaptedSetter = adapt_method_setter(setter);
			create_property<PropT>(name, adaptedGetter, adaptedSetter, flags, &sge::get_type<ContextT>());
			return std::move(*this);
		}

		/* Creates a property with a method getter and method setter, where both require a context. */
		template <typename GetRetT, typename GetContextT, typename SetRetT, typename SetContextT, typename SetArgT>
		NativeTypeInfoBuilder&& property(
			const char* name,
			GetRetT(T::*getter)(GetContextT)const,
			SetRetT(T::*setter)(SetContextT, SetArgT),
			PropertyFlags flags = PF_NONE)
		{
			using PropT = std::decay_t<GetRetT>;
			using ContextT = std::decay_t<GetContextT>;
			static_assert(std::is_same<PropT, std::decay_t<SetArgT>>::value, "Getter and setter use different property types.");
			static_assert(std::is_same<ContextT, std::decay_t<SetContextT>>::value, "Getter and setter use different context types.");

			auto adaptedGetter = adapt_method_getter(getter);
			auto adaptedSetter = adapt_method_setter(setter);
			create_property<PropT>(name, getter, setter, flags, &sge::get_type<ContextT>());
			return std::move(*this);
		}

		/* Registers a field. */
		template <typename FieldT>
		NativeTypeInfoBuilder&& field(
			const char* name,
			FieldT T::*field,
			FieldFlags flags = FF_NONE)
		{
			create_field(name, field, flags);
			return std::move(*this);
		}

		/* Registers a field, and a corresponding property. */
		template <typename FieldT>
		NativeTypeInfoBuilder&& field_property(
			const char* name,
			FieldT T::*field,
			FieldFlags fieldFlags = FF_NONE,
			PropertyFlags propertyFlags = PF_NONE)
		{
			create_field(name, field, fieldFlags);

			PropertyInfo::Data basePropData;
			basePropData.type = &sge::get_type<FieldT>();
			basePropData.flags = propertyFlags;

			NativePropertyInfo::Data propData;
			propData.getter = create_field_getter(field);
			propData.setter = create_field_setter(field);
			propData.mutate = create_field_mutate(field);

			type_data.properties.insert(std::make_pair(name, NativePropertyInfo{ std::move(basePropData), std::move(propData) }));

			return std::move(*this);
		}

		/* Registers a field, and a corresponding readonly property. */
		template <typename FieldT>
		NativeTypeInfoBuilder&& field_readonly_property(
			const char* name,
			FieldT T::*field,
			FieldFlags fieldFlags = FF_NONE,
			PropertyFlags propertyFlags = PF_NONE)
		{
			create_field(name, field, fieldFlags);

			PropertyInfo::Data basePropData;
			basePropData.type = &sge::get_type<FieldT>();
			basePropData.flags = propertyFlags;

			NativePropertyInfo::Data propData;
			create_field_getter(propData, field);

			type_data.properties.insert(std::make_pair(name, NativePropertyInfo{ std::move(basePropData), std::move(propData) }));

			return std::move(*this);
		}

	private:

		template <typename FieldT>
		static std::size_t get_field_offset(FieldT T::*field)
		{
			// Bit of a hack, but necessary. If this becomes problematic, I can replace the field offset with a getter/setter std::function pair or something.
			// Though that would be much less performant.
			alignas(T&) const char fake[sizeof(T)] = {};
			const char* pField = reinterpret_cast<const char*>(&(reinterpret_cast<const T*>(&fake)->*field));

			return pField - &fake[0];
		}

		template <typename PropT, typename GetFn, typename SetFn>
		void create_property(const char* name, GetFn getter, SetFn setter, PropertyFlags flags, const TypeInfo* contextType)
		{
			PropertyInfo::Data basePropData;
			basePropData.type = &sge::get_type<PropT>();
			basePropData.context_type = contextType;
			basePropData.flags = flags;

			NativePropertyInfo::Data propData;
			propData.getter = create_getter<PropT>(getter);
			propData.setter = create_setter<PropT>(setter);
			propData.mutate = create_mutate<PropT>(getter, setter);

			type_data.properties.insert(std::make_pair(name, NativePropertyInfo{ std::move(basePropData), std::move(propData) }));
		}

		template <typename PropT, typename GetFn>
		void create_readonly_property(const char* name, GetFn getter, PropertyFlags flags, const TypeInfo* contextType)
		{
			PropertyInfo::Data basePropData;
			basePropData.type = &sge::get_type<PropT>();
			basePropData.context_type = contextType;
			basePropData.flags = flags;

			NativePropertyInfo::Data propData;
			propData.getter = create_getter<PropT>(getter);

			type_data.properties.insert(std::make_pair(name, NativePropertyInfo{ std::move(basePropData), std::move(propData) }));
		}

		template <typename FieldT>
		void create_field(const char* name, FieldT T::*field, FieldFlags flags)
		{
			FieldInfo::Data fieldData;
			fieldData.flags = flags;
			fieldData.type = &sge::get_type<FieldT>();
			fieldData.offset = get_field_offset(field);

			type_data.fields.insert(std::make_pair(name, std::move(fieldData)));
		}

		template <typename GetFn>
		static auto adapt_function_getter(GetFn getter)
		{
			using FnTraits = stde::function_traits<GetFn>;
			using RetT = typename FnTraits::return_type;
			using PContextT = tmp::car_n<typename FnTraits::arg_types, 1>;

			return [getter](const T* self, const void* context) -> RetT {
				return getter(self, static_cast<PContextT>(context));
			};
		}

		template <typename RetT>
		static auto adapt_method_getter(RetT(T::*getter)()const)
		{
			return [getter](const T* self, const void* /*context*/) -> RetT {
				return (self->*getter)();
			};
		}

		template <typename RetT, typename ContextArgT>
		static auto adapt_method_getter(RetT(T::*getter)(ContextArgT)const)
		{
			using ContextT = std::decay_t<ContextArgT>;

			return [getter](const T* self, const void* context) -> RetT {
				return (self->*getter)(*static_cast<const ContextT*>(context));
			};
		}

		template <typename SetFn>
		static auto adapt_function_setter(SetFn setter)
		{
			using FnTraits = stde::function_traits<SetFn>;
			using PContextT = tmp::car_n<typename FnTraits::arg_types, 1>;
			using PPropT = tmp::car_n<typename FnTraits::arg_types, 2>;

			return [setter](T* self, void* context, PPropT value) -> void {
				setter(self, static_cast<PContextT>(context), value);
			};
		}

		template <typename RetT, typename PropArgT>
		static auto adapt_method_setter(RetT(T::*setter)(PropArgT))
		{
			using PropT = std::decay_t<PropArgT>;

			return [setter](T* self, void* /*context*/, const PropT* value) -> void {
				(self->*setter)(*value);
			};
		}

		template <typename RetT, typename ContextArgT, typename PropArgT>
		static auto adapt_method_setter(RetT(T::*setter)(ContextArgT, PropArgT))
		{
			using ContextT = std::decay_t<ContextArgT>;
			using PropT = std::decay_t<PropArgT>;

			return [setter](T* self, void* context, const PropT* value) -> void {
				(self->*setter)(*static_cast<ContextT*>(context), *value);
			};
		}

		template <typename PropT, typename GetFn>
		static auto create_getter(GetFn getter)
		{
			return [getter](const void* self, const void* context, PropertyInfo::GetterOutFn out) -> void {
				const PropT& value = getter(static_cast<const T*>(self), context);
				out(value);
			};
		}

		template <typename PropT, typename SetFn>
		static auto create_setter(SetFn setter)
		{
			return [setter](void* self, void* context, const void* value) -> void {
				setter(static_cast<T*>(self), context, static_cast<const PropT*>(value));
			};
		}

		template <typename PropT, typename GetFn, typename SetFn>
		static auto create_mutate(GetFn getter, SetFn setter)
		{
			return [getter, setter](void* self, void* context, PropertyInfo::MutatorFn mutator) -> void {
				PropT prop = getter(static_cast<const T*>(self), context);
				mutator(prop);
				setter(static_cast<T*>(self), context, &prop);
			};
		}

		template <typename FieldT>
		static auto create_field_getter(FieldT T::*field)
		{
			return [field](const void* self, const void* /*context*/, PropertyInfo::GetterOutFn out) -> void {
				out(static_cast<const T*>(self)->*field);
			};
		}

		template <typename FieldT>
		static auto create_field_setter(FieldT T::*field)
		{
			return [field](void* self, void* /*context*/, const void* value) -> void {
				static_cast<T*>(self)->*field = *static_cast<const FieldT*>(value);
			};
		}

		template <typename FieldT>
		static auto create_field_mutate(FieldT T::*field)
		{
			return [field](void* self, void* /*context*/, PropertyInfo::MutatorFn mutate) -> void {
				mutate(static_cast<T*>(self)->*field);
			};
		}

		template <typename F>
		auto set_init() -> std::enable_if_t<std::is_default_constructible<F>::value>
		{
			type_data.init = [](void* addr) -> void {
				new(addr) F();
			};
		}

		template <typename F>
		auto set_init() -> std::enable_if_t<!std::is_default_constructible<F>::value>
		{
		}

		template <typename F>
		auto set_copy_init() -> std::enable_if_t<std::is_copy_constructible<F>::value>
		{
			type_data.copy_init = [](void* addr, const void* copy) -> void {
				new(addr) F(*static_cast<const F*>(copy));
			};
		}

		template <typename F>
		auto set_copy_init() -> std::enable_if_t<!std::is_copy_constructible<F>::value>
		{
		}

		template <typename F>
		auto set_move_init() -> std::enable_if_t<std::is_move_constructible<F>::value>
		{
			type_data.move_init = [](void* addr, void* move) -> void {
				new(addr) T(std::move(*static_cast<T*>(move)));
			};
		}

		template <typename F>
		auto set_move_init() -> std::enable_if_t<!std::is_move_constructible<F>::value>
		{
		}

		template <typename F>
		auto set_copy_assign() -> std::enable_if_t<std::is_copy_assignable<F>::value>
		{
			type_data.copy_assign = [](void* self, const void* copy) -> void {
				*static_cast<T*>(self) = *static_cast<const T*>(copy);
			};
		}

		template <typename F>
		auto set_copy_assign() -> std::enable_if_t<!std::is_copy_assignable<F>::value>
		{
		}

		template <typename F>
		auto set_move_assign() -> std::enable_if_t<std::is_move_assignable<F>::value>
		{
			type_data.move_assign = [](void* self, void* move) -> void {
				*static_cast<T*>(self) = std::move(*static_cast<T*>(move));
			};
		}

		template <typename F>
		auto set_move_assign() -> std::enable_if_t<!std::is_move_assignable<F>::value>
		{
		}

		template <typename F>
		auto set_drop() -> std::enable_if_t<std::is_destructible<F>::value>
		{
			type_data.drop = [](void* self) -> void {
				static_cast<T*>(self)->~T();
			};
		}

		template <typename F>
		auto set_drop() -> std::enable_if_t<!std::is_destructible<F>::value>
		{
		}

		//////////////////
		///   Fields   ///
	public:

		TypeInfo::Data base_type_data;
		NativeTypeInfo::Data type_data;
	};

	template <class I>
	struct InterfaceInfoBuilder final
	{
		////////////////////////
		///   Constructors   ///
	public:

		InterfaceInfoBuilder(std::string name)
		{
			interface_data.name = std::move(name);
		}

		//////////////////
		///   Fields   ///
	public:

		InterfaceInfo::Data interface_data;

		///////////////////
		///   Methods   ///
	public:

		template <typename T>
		InterfaceInfoBuilder&& implemented_for()
		{
			interface_data.implementations.insert(std::make_pair(&sge::get_type<T>(), &I::template get_impl<T>()));
			return std::move(*this);
		}
	};
}

//////////////////
///   Macros   ///

/* Use this macro in the source file for a type, in order to deine its reflection data. */
#define SGE_REFLECT_TYPE(TYPE)		const ::sge::NativeTypeInfo TYPE::type_info = ::sge::NativeTypeInfoBuilder<TYPE>(#TYPE)

/* Use this macro in the source file for an interface, in order to define its pre-existing implementations. */
#define SGE_REFLECT_INTERFACE(INTERF)	const ::sge::InterfaceInfo INTERF::interface_info = ::sge::InterfaceInfoBuilder<INTERF>(#INTERF)