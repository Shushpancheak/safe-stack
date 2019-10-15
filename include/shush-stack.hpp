#pragma once
#include <cinttypes>
#include "shush-logs.hpp"
#include "shush-dump.hpp"

namespace shush {
namespace stack {

// In T elements
inline static const size_t DEFAULT_RESERVED_SIZE = 2048;
inline static const size_t DEFAULT_INITIAL_SIZE  = 10;

inline static const size_t   CANARY_SIZE   = sizeof(uint64_t);
inline static const uint64_t CANARY_VALUE  = 0xDEDDA1C0FFEE;

inline static const size_t   HASH_SIZE     = sizeof(uint64_t);
inline static const size_t   HASH_POS      = CANARY_SIZE;

inline static const size_t   CUR_SIZE_SIZE = sizeof(size_t);
inline static const size_t   CUR_SIZE_POS  = HASH_POS + HASH_SIZE;

inline static const size_t   BUF_SIZE_SIZE = sizeof(size_t);
inline static const size_t   BUF_SIZE_POS  = CUR_SIZE_POS + CUR_SIZE_SIZE;

inline static const size_t   BUF_POS       = BUF_SIZE_POS + BUF_SIZE_SIZE;

inline static const char     POISON_VALUE  = '#';

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
// - - - - - - - - - - - - - ERROR CODES - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
enum Errc {
  ASSERT_FAILED                    = -1,
  THIS_PTR_IS_NULLPTR              = 0,
  CORRUPTED_FIRST_CANARY           = 1,
  CORRUPTED_SECOND_CANARY          = 2,
  HASH_NOT_THE_SAME                = 3,
  CUR_SIZE_IS_BIGGER_THAN_BUF      = 4,
  UNINITIALIZED_CELL_IS_NOT_POISON = 5,
  POP_ON_0_SIZE                    = 6,
  REALLOCATION_IN_STATIC_STACK     = 7
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
// - - - - - - - - - - - - - - DYNAMIC - - - - - - - - - - - - - - - - - -
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

/**
 * STRUCTURE:
 * [CANARY][HASH][CUR_SIZE][BUFFER_SIZE][B - U - F - F - E - R][CANARY]
 */
template <class T>
class SafeStack {
public:
  SafeStack();
  ~SafeStack();

  SafeStack(const SafeStack& stack)            = delete;
  SafeStack(SafeStack&& stack)                 = delete;
  SafeStack& operator=(const SafeStack& stack) = delete;
  SafeStack& operator=(SafeStack&& stack)      = delete;

  void Push(const T& item);
  void Push(T&& item);

  T Pop();

  /**
   * Get the size of current used space.
   */
  size_t GetCurSize();
  /**
   * Get current capacity of the stack.
   */
  size_t GetBufSize();

  void Ok();

protected:
  /**
   * Message for Ok() calls.
   */
  std::string GetDumpMessage(Errc error_code);
  std::string GetDumpMessage();
  std::string GetErrorMessage(Errc error_code);

  /**
   * Fills both sides of buffer with canaries.
   */
  void FillCanaries(size_t all_buffer_size);
  /**
   * Sets current size of stack in buffer.
   */
  void SetCurSizeVal(size_t cur_size);
  /**
   * Sets capacity of the stack in buffer.
   */
  void SetBufferSizeVal(size_t buffer_size);
  /**
   * Fills given range with poison values.
   */
  void FillWithPoison(char* from, char* to);
  /**
   * Calculates and places hash into corresponding place
   */
  void CalculateAndPlaceHash(size_t all_buffer_size);
  void CalculateAndPlaceHash();

  uint64_t GetHashValue();
  uint64_t GetFirstCanary();
  uint64_t GetSecondCanary();
  /**
   * Get size of all allocated space. In chars.
   */
  size_t   GetAllBufferSize();

  /**
   * Get poison value as if it was T.
   */
  constexpr T GetPoisonValue();
  /**
   * Checks whether the given value is poison.
   */
  bool        IsPoison(const T& val);

  uint64_t CalculateHash(size_t all_buffer_size);
  uint64_t CalculateHash();

  T GetElement(size_t ind);

  /**
   * Doubles the capacity and reallocates the whole buffer.
   */
  void ReallocateDoubleSize();

  char* buf_;
  logs::Logger logger_;
  static size_t stacks_count;
};

template <class T>
constexpr T SafeStack<T>::GetPoisonValue() {
  char elem[sizeof(T)];
  for (size_t i = 0; i < sizeof(T); ++i) {
    elem[i] = POISON_VALUE;
  }
  return *reinterpret_cast<T*>(&elem);
}

template <class T>
SafeStack<T>::SafeStack()
    : logger_("shush-stack-" + std::to_string(stacks_count)) {
  logger_.Dbg("Construction of the DYNAMIC stack started.");
  logger_.Dbg("The type that is held in the stack is " +
              std::string(typeid(T).name()) + ", and its size is " +
              std::to_string(sizeof(T)) + ".");

  const size_t all_size = CANARY_SIZE + HASH_SIZE +
                          CUR_SIZE_SIZE + BUF_SIZE_SIZE +
                          DEFAULT_INITIAL_SIZE * sizeof(T) +
                          CANARY_SIZE;

  buf_ = new char[all_size];
  logger_.Dbg("Allocated " + std::to_string(all_size) +
              " bytes of memory for DYNAMIC buffer.");

  SetBufferSizeVal(DEFAULT_INITIAL_SIZE);
  SetCurSizeVal(0);
  FillCanaries(all_size);
  FillWithPoison(buf_ + BUF_POS, buf_ + all_size - CANARY_SIZE);
  CalculateAndPlaceHash(all_size);

  logger_.Dbg("Construction of the stack completed.");
  ++stacks_count;
}

template <class T>
SafeStack<T>::~SafeStack() {
  logger_.Dbg("Destructing stack by deleting the buffer...");
  delete[] buf_;
  logger_.Dbg("Destruction is complete. Bye-bye!");
  --stacks_count;
}

template <class T>
void SafeStack<T>::Push(const T& item) { VERIFIED
  logger_.Dbg("Pushing an element that is a const ref...");

  if (GetCurSize() == GetBufSize()) {
    logger_.Dbg("The size of buffer is equal to current size! Starting the reallocation...");
    ReallocateDoubleSize();
  }

  const size_t pos = BUF_POS + GetCurSize() * sizeof(T);
  new(buf_ + pos) T(item);
  logger_.Dbg("Placed the new element in cell starting from " +
              std::to_string(pos) + ".");

  const size_t cur_size = GetCurSize() + 1;
  SetCurSizeVal(cur_size);
  logger_.Dbg("Pushing of const ref element is complete. The new cur size is " + std::to_string(cur_size) + ".");

  CalculateAndPlaceHash();
}

template <class T>
void SafeStack<T>::Push(T&& item) { VERIFIED
  logger_.Dbg("Pushing an element that is an rvalue...");

  if (GetCurSize() == GetBufSize()) {
    logger_.Dbg("The size of buffer is equal to current size! Starting the reallocation...");
    ReallocateDoubleSize();
  }

  const size_t pos = BUF_POS + GetCurSize() * sizeof(T);
  new(buf_ + pos) T(std::move(item));
  logger_.Dbg("Placed the new element in cell starting from " +
              std::to_string(pos) + ".");

  const size_t cur_size = GetCurSize() + 1;
  SetCurSizeVal(cur_size);
  logger_.Dbg("Pushing of an rvalue element is complete. The new cur size is " + std::to_string(cur_size) + ".");

  CalculateAndPlaceHash();
}

template <class T>
T SafeStack<T>::Pop() { VERIFIED
  logger_.Dbg("Started popping the element...");

  const size_t size = GetCurSize();
  if (size == 0) {
    logger_.Log("Oh no, the size of stack is already 0! Aborting...");
    MASSERT(true, Errc::POP_ON_0_SIZE); //TODO maybe make non debug assert
  }

  const size_t pos = BUF_POS + (size - 1) * sizeof(T);
  T res = *reinterpret_cast<T*>(buf_ + pos);
  logger_.Dbg("Got the value");

  FillWithPoison(buf_ + pos, buf_ + pos + sizeof(T));

  SetCurSizeVal(size - 1);
  logger_.Dbg("Popping is complete. The new size is " +
              std::to_string(size - 1));

  CalculateAndPlaceHash();

  return res;
}

template <class T>
void SafeStack<T>::Ok() {
  logger_.Dbg("Started verification procedure...");

  MASSERT(this != nullptr,                   Errc::THIS_PTR_IS_NULLPTR);
  MASSERT(GetFirstCanary() == CANARY_VALUE,  Errc::CORRUPTED_FIRST_CANARY);
  MASSERT(GetSecondCanary() == CANARY_VALUE, Errc::CORRUPTED_SECOND_CANARY);
  MASSERT(GetHashValue() == CalculateHash(), Errc::HASH_NOT_THE_SAME);
  MASSERT(GetCurSize() <= GetBufSize(),   Errc::CUR_SIZE_IS_BIGGER_THAN_BUF);

  const size_t cur_size_bytes = BUF_POS + GetCurSize() * sizeof(T);
  for (size_t i = BUF_POS, all_size = GetAllBufferSize() - CANARY_SIZE;
       i < all_size; i += sizeof(T)) {
    if (i < cur_size_bytes) {
      if (IsPoison(*reinterpret_cast<T*>(buf_ + i))) {
        logger_.Dbg("WARNING: element " + std::to_string((i - BUF_POS) / sizeof(T)) + " is equal to poison value");
      }
    } else {
      MASSERT(IsPoison(*reinterpret_cast<T*>(buf_ + i)), 
                       Errc::UNINITIALIZED_CELL_IS_NOT_POISON);
    }
  }
}

template <class T>
std::string SafeStack<T>::GetDumpMessage(Errc error_code) {
  logger_.Log("WARNING: Oh-oh, it appears a GetDumpMessage was invoked!");
  std::string str = "\n- - - - - - DUMP MESSAGE FROM SHUSH::STACK- - - - - - \n";

  str += "this address: " + std::to_string(reinterpret_cast<size_t>(this)) + ".\n";
  str += "Error code == " + std::to_string(error_code) + " (";
  str += GetErrorMessage(error_code);
  str += ")\n\n";

  str += "Byte representation of the stack:\n" + std::string(buf_) + "\n\n";

  str += "Detailed:\n";
  str += dump::GetBadGoodStr(GetFirstCanary() == CANARY_VALUE) +
         "[CANARY] == " + std::to_string(GetFirstCanary()) + "\n";
  str += dump::GetBadGoodStr(CalculateHash() == GetHashValue()) +
         "[HASH] == " + std::to_string(CalculateHash()) + "\n";
  str += dump::GetBadGoodStr(GetCurSize() <= GetBufSize()) +
         "[CUR_SIZE] == " + std::to_string(GetCurSize()) + "\n";
  str += dump::GetBadGoodStr(GetCurSize() <= GetBufSize()) +
         "[BUF_SIZE] == " + std::to_string(GetBufSize()) + "\n";

  using std::to_string;
  const size_t cur_size = GetCurSize();
  for (size_t i = 0; i < cur_size; ++i) {
    str += std::string(IsPoison(GetElement(i)) ? "(WARNING) " : "(GOOD) ")
           + "[" + to_string(i) + "] == " +
           to_string(GetElement(i)) + "\n";
  }

  for (size_t i = cur_size, buf_size = GetBufSize(); i < buf_size; ++i) {
    str += dump::GetBadGoodStr(IsPoison(GetElement(i))) +
           "[" + to_string(i) + "] == " +
           to_string(GetElement(i)) + "\n";
  }

  str += dump::GetBadGoodStr(GetSecondCanary() == CANARY_VALUE) +
         "[CANARY] == " + std::to_string(GetSecondCanary()) + "\n";

  str += "\n- - - -END OF DUMP MESSAGE FROM SHUSH::STACK- - - - - - \n";

  return str;
}

template <class T>
std::string SafeStack<T>::GetDumpMessage() {
  return GetDumpMessage(Errc::ASSERT_FAILED);
}

template <class T>
std::string SafeStack<T>::GetErrorMessage(Errc error_code) {
  switch (error_code) {
    case Errc::ASSERT_FAILED: {
      return "assertion failed";
    }
    case Errc::THIS_PTR_IS_NULLPTR: {
      return "[this] pointer points to nullptr. Have you forgot to initialize the object?";
    }
    case Errc::CORRUPTED_FIRST_CANARY: {
      return "first [CANARY] was corrupted. Perhaps, someone tried to overwrite it";
    }
    case Errc::CORRUPTED_SECOND_CANARY: {
      return "second [CANARY] was corrupted. Perhaps, someone tried to overwrite it";
    }
    case Errc::CUR_SIZE_IS_BIGGER_THAN_BUF: {
      return "current size value of stack is bigger than buffer size value";
    }
    case Errc::HASH_NOT_THE_SAME: {
      return "calculated hash is not equal to what is stored";
    }
    case Errc::UNINITIALIZED_CELL_IS_NOT_POISON: {
      return "one of the uninitialized cells is not equal to poison value. Perhaps, someone tried to overwrite it";
    }
    case Errc::POP_ON_0_SIZE: {
      return "the size of the stack was 0, and a Pop() method has been called.";
    }
    case Errc::REALLOCATION_IN_STATIC_STACK: {
      return "static stack overflow. Consider increasing its capacity or switching to dynamic stack.";
    }
    default: {
      return "UNKNOWN ERROR CODE";
    }
  }
}

template <class T>
bool SafeStack<T>::IsPoison(const T& val) {
  for (size_t i = 0; i < sizeof(T); ++i) {
    if (reinterpret_cast<const char*>(&val)[i] != POISON_VALUE) {
      return false;
    }
  }
  return true;
}

template <class T>
void SafeStack<T>::FillCanaries(size_t all_buffer_size) {
  memcpy(buf_, &CANARY_VALUE, CANARY_SIZE);
  memcpy(buf_ + all_buffer_size - CANARY_SIZE,
         &CANARY_VALUE, CANARY_SIZE);

  logger_.Dbg("Filled canaries inside " +
              std::to_string(all_buffer_size) + " bytes.");
  CalculateAndPlaceHash();
}

template <class T>
void SafeStack<T>::SetCurSizeVal(size_t cur_size) {
  memcpy(buf_ + CUR_SIZE_POS, &cur_size, CUR_SIZE_SIZE);

  logger_.Dbg("Set current size of the stack to " +
              std::to_string(cur_size) + ".");
  CalculateAndPlaceHash();
}

template <class T>
void SafeStack<T>::SetBufferSizeVal(size_t buffer_size) {
  memcpy(buf_ + BUF_SIZE_POS, &buffer_size, BUF_SIZE_SIZE);

  logger_.Dbg("Set buffer size value of the stack to " +
              std::to_string(buffer_size) + ".");
  CalculateAndPlaceHash();
}

template <class T>
void SafeStack<T>::
FillWithPoison(char* from, char* to) {
  for (auto i = from; i != to; ++i) {
    *i = POISON_VALUE;
  }

  logger_.Dbg("Filled addresses from " +
              std::to_string(reinterpret_cast<size_t>(from)) + " to " +
              std::to_string(reinterpret_cast<size_t>(to)) + " with Poison.");
  CalculateAndPlaceHash();
}

template <class T>
void SafeStack<T>::CalculateAndPlaceHash(
  const size_t all_buffer_size) {
  uint64_t hash = CalculateHash(all_buffer_size);
  memcpy(buf_ + HASH_POS, &hash, HASH_SIZE);

  logger_.Dbg("Placed hash.");
}

template <class T>
void SafeStack<T>::CalculateAndPlaceHash() {
  CalculateAndPlaceHash(GetAllBufferSize());
}

template <class T>
uint64_t SafeStack<T>::CalculateHash(size_t all_buffer_size) {
  const uint64_t hash = std::hash<size_t>()(reinterpret_cast<size_t>(this))+
  std::hash<std::string_view>() (
    std::string_view(buf_, HASH_POS)
  ) +
  std::hash<std::string_view>() (
    std::string_view(buf_ + HASH_POS + HASH_SIZE, 
                     all_buffer_size - HASH_SIZE - HASH_POS + 1)
  );

  logger_.Dbg("Calculated hash. Its value: " + std::to_string(hash));

  return hash;
}

template <class T>
uint64_t SafeStack<T>::CalculateHash() {
  return CalculateHash(GetAllBufferSize());
}

template <class T>
T SafeStack<T>::GetElement(size_t ind) {
  return *reinterpret_cast<T*>(buf_ + BUF_POS + ind * sizeof(T));
}

template <class T>
void SafeStack<T>::ReallocateDoubleSize() { VERIFIED
  const size_t all_size = GetAllBufferSize();
  const size_t buf_t_size = GetBufSize();
  const size_t new_all_size = all_size + buf_t_size * sizeof(T);
  char* new_buf = new char[new_all_size];

  logger_.Dbg("Started reallocating stack. Initial all_size = " +
              std::to_string(all_size) + ", new_all_size = " +
              std::to_string(new_all_size));

  logger_.Dbg("Now starting to call constructors in allocated space.");

  for (size_t i = 0; i < buf_t_size; ++i) {
    const size_t pos = BUF_POS + i * sizeof(T);
    new(new_buf + pos) T(std::move(*reinterpret_cast<T*>(buf_ + pos)));
  }

  logger_.Dbg("Deleting the old buffer...");
  delete[] buf_;
  buf_ = new_buf;

  SetBufferSizeVal(buf_t_size * 2);
  SetCurSizeVal(buf_t_size);
  FillCanaries(new_all_size);
  FillWithPoison(buf_ + all_size - CANARY_SIZE,
                 buf_ + new_all_size - CANARY_SIZE);

  logger_.Dbg("Reallocation completed.");

  CalculateAndPlaceHash(new_all_size);
}

template <class T>
uint64_t SafeStack<T>::GetFirstCanary() {
  return *reinterpret_cast<uint64_t*>(buf_);
}

template <class T>
uint64_t SafeStack<T>::GetSecondCanary() {
  return *reinterpret_cast<uint64_t*>(buf_ + GetAllBufferSize()
                                           - CANARY_SIZE);
}

template <class T>
size_t SafeStack<T>::GetCurSize() {
  return *reinterpret_cast<size_t*>(buf_ + CUR_SIZE_POS);
}

template <class T>
size_t SafeStack<T>::GetBufSize() {
  return *reinterpret_cast<size_t*>(buf_ + BUF_SIZE_POS);
}

template <class T>
uint64_t SafeStack<T>::GetHashValue() {
  return *reinterpret_cast<uint64_t*>(buf_ + HASH_POS);
}

template <class T>
size_t SafeStack<T>::GetAllBufferSize() {
  return GetBufSize() * sizeof(T) + CANARY_SIZE * 2 +
         HASH_SIZE + CUR_SIZE_SIZE + BUF_SIZE_SIZE;
}

template <class T>
size_t SafeStack<T>::stacks_count(0);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
// - - - - - - - - - - - - - - STATIC- - - - - - - - - - - - - - - - - - - 
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

template<class T, size_t ReservedSize = DEFAULT_RESERVED_SIZE>
class SafeStackStatic : public SafeStack<T> {
public:
  SafeStackStatic();
  ~SafeStackStatic();

  SafeStackStatic(const SafeStackStatic& stack)            = delete;
  SafeStackStatic(SafeStackStatic&& stack)                 = delete;
  SafeStackStatic& operator=(const SafeStackStatic& stack) = delete;
  SafeStackStatic& operator=(SafeStackStatic&& stack)      = delete;

private:
  void ReallocateDoubleSize();

  char buf_static_[CANARY_SIZE + HASH_SIZE +
                   CUR_SIZE_SIZE + BUF_SIZE_SIZE +
                   ReservedSize * sizeof(T) + CANARY_SIZE] {};
};

template <class T, size_t ReservedSize>
SafeStackStatic<T, ReservedSize>::SafeStackStatic() {
  this->logger_.Dbg("Construction of the STATIC stack started.");
  this->logger_.Dbg("The type that is held in the stack is " +
              std::string(typeid(T).name()) + ", and its size is " +
              std::to_string(sizeof(T)) + ".");
  this->logger_.Dbg("The reserved size is " + std::to_string(ReservedSize));

  const size_t all_size = CANARY_SIZE + HASH_SIZE +
                  CUR_SIZE_SIZE + BUF_SIZE_SIZE +
                  ReservedSize * sizeof(T) + CANARY_SIZE;

  this->logger_.Dbg("Setting buf_ ptr to static buf_ pointer.");
  this->buf_ = buf_static_;
  this->SetBufferSizeVal(ReservedSize);
  this->SetCurSizeVal(0);
  this->FillCanaries(all_size);
  this->FillWithPoison(this->buf_ + BUF_POS,
                       this->buf_ + all_size - CANARY_SIZE);
  this->CalculateAndPlaceHash(all_size);
}

template <class T, size_t ReservedSize>
SafeStackStatic<T, ReservedSize>::~SafeStackStatic() {
  this->logger_.Dbg("Destruction of the safe STATIC stack has been invoked.");
  this->logger_.Dbg("Untying the pointer buf_ to nullptr...");

  this->buf_ = nullptr;
}

template <class T, size_t ReservedSize>
void SafeStackStatic<T, ReservedSize>::ReallocateDoubleSize() {
  this->logger_.Log("Oh no! Reallocation was called in STATIC stack! Aborting...");
  MASSERT(true, Errc::REALLOCATION_IN_STATIC_STACK);
  //TODO make non debug assert
}

}
}