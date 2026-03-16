#include <QCoreApplication>
#include <gtest/gtest.h>

// Точка входа для Google Test.
// QCoreApplication нужен, так как часть кода использует Qt (QString,
// QCryptographicHash и т.д.) — без него некоторые Qt-модули могут не работать.
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
