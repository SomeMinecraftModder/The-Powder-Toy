#ifndef TPT_COMPAT_H
#define TPT_COMPAT_H

// From https://stackoverflow.com/questions/17902405/how-to-implement-make-unique-function-in-c11

#if !(__cplusplus >= 201402L || defined(_MSC_VER))
namespace std {
	template<class T> struct _Unique_if {
		typedef unique_ptr<T> _Single_object;
	};

	template<class T> struct _Unique_if<T[]> {
		typedef unique_ptr<T[]> _Unknown_bound;
	};

	template<class T, size_t N> struct _Unique_if<T[N]> {
		typedef void _Known_bound;
	};

	template<class T, class... Args>
		typename _Unique_if<T>::_Single_object
		make_unique(Args&&... args) {
			return unique_ptr<T>(new T(std::forward<Args>(args)...));
		}

	template<class T>
		typename _Unique_if<T>::_Unknown_bound
		make_unique(size_t n) {
			typedef typename remove_extent<T>::type U;
			return unique_ptr<T>(new U[n]());
		}

	template<class T, class... Args>
		typename _Unique_if<T>::_Known_bound
		make_unique(Args&&...) = delete;
}
#endif

#endif
