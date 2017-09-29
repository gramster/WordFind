// charbuff - a simple character buffer class
//
// This is intended for use wherever we have a:
//
// char *x = new char[size];
//
// type resource, where the resource is deallocated in the
// same scope block.
//
// By encapsulating this in a simple class, we not only allow
// the resource to be automatically deallocated when it goes out
// of scope, but can also add features such as overrun/underrun
// checks.
//
// To use this, we would replace the example above with:
//
// DCL_CHARBUFF(x, size);
//
// There are other macros for dynamic allocation, construction, etc.
//

#ifndef CHARBUFF_H
#define CHARBUFF_H

class CharBuff
{
    char *buff;
    unsigned size;
    const char *file;
    int line;
  public:
    char &operator[](unsigned i) const;
    operator const char*() const;
    operator char*() const;
    CharBuff(unsigned size_in, const char *file_in = 0, int line_in = 0);
    void Set(const char *v, const char *file_in = 0, int line_in = 0);
    ~CharBuff();
};

inline char &CharBuff::operator[](unsigned i) const
{
    return buff[i];
}

inline CharBuff::operator const char*() const
{
    return buff;
}

inline CharBuff::operator char*() const
{
    return buff;
}

#define DCL_CHARBUFF(var, sz)	CharBuff var(sz, __FILE__, __LINE__)
#define NEW_CHARBUFF(var, sz)	do { var = new CharBuff(sz, __FILE__, __LINE__); } while(0)
#define CON_CHARBUFF(var, sz)	var(sz, __FILE__, __LINE__)
#define SET_CHARBUFF(var, v)	var.Set(v, __FILE__, __LINE__)
#define SET_CHARBUFFP(var, v)	var->Set(v, __FILE__, __LINE__)

#endif

