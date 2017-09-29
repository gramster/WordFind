#ifndef BLOWFISH_H
#define BLOWFISH_H

const short bf_N = 16;

class Blowfish
{
private:
  unsigned short ok;
  int blocksize;

  unsigned long bf_P[bf_N + 2];

  unsigned long bf_S[4][256];

  union aword
  {
    unsigned long word;
    unsigned char byte [4];
    struct
    {
      unsigned int byte3:8;
      unsigned int byte2:8;
      unsigned int byte1:8;
      unsigned int byte0:8;
    } w;
  };

public:

    Blowfish();
    ~Blowfish();

    int Ok() { return ok; }
    void EncipherBlock(unsigned long *xl, unsigned long *xr);
    void DecipherBlock(unsigned long *xl, unsigned long *xr);
    int SetKey(const unsigned char *key, const int len);

    inline int Keylen() const
    { 
        return 56; 
    }
    inline int BlockSize() const
    { 
        return 8; 
    }
   void Encipher(unsigned char *buf, const int len);
   void Decipher(unsigned char *buf, const int len);    
};



#endif BLOWFISH_H
