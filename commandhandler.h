#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <QString>
#include <QStringList>

class CommandHandler
{
public:
    CommandHandler();

    QString handleRequest(const QString& command,
                          const QStringList& parts,
                          const QString& clientRole = "", // Добавляем параметры
                          int clientUserId = -1);

private:
    // User commands
    QString handleExtendRental(const QStringList& parts);
    QString handleViewBookStats(const QStringList& parts);
    QString handleViewUserInfo(const QStringList& parts);

    // Librarian commands
    QString handleAssignBook(const QStringList& parts);
    QString handleUnassignBook(const QStringList& parts);
    QString handleViewUserDebts(const QStringList& parts);
    QString handleGetUserInfo(const QStringList& parts);
    QString handleAddBookToLibrary(const QStringList& parts);
    QString handleUpdateBookInfo(const QStringList& parts);

    // Common commands
    QString handleAuth(const QStringList& parts);
    QString handleReg(const QStringList& parts);

    // обработчик для поиска книг
    QString handleSearchBooks(const QStringList& parts);

    // Новые обработчики для аннотаций
    QString handleGetBookAnnotation(const QStringList& parts);
    QString handleUpdateBookAnnotation(const QStringList& parts);

    // Новый обработчик для получения списка пользователей
    QString handleGetAllUsers(const QStringList& parts);

    QString handleImportBooksCsv(const QStringList& parts);
    QString handleExportBooksCsv(const QStringList& parts);

    // Новые обработчики для управления пользователями
    QString handleBlockUser(const QStringList& parts);
    QString handleUnblockUser(const QStringList& parts);
    QString handleResetUserPassword(const QStringList& parts);
    QString handleUpdateUserEmail(const QStringList& parts);

    // Новый обработчик для истории аренды
    QString handleGetRentalHistory(const QStringList& parts);

    // Новый обработчик для статистики
    QString handleGetLibraryStats(const QStringList& parts);


    // Новый обработчик для ДЕТАЛЬНЫХ ОТЧЕТОВ
    QString handleGetStatisticsReport(const QStringList& parts);

    // Запрос списка жанров
    QString handleGetGenresList(const QStringList& parts);
};

#endif // COMMANDHANDLER_H
