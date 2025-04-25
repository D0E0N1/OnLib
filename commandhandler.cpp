#include "commandhandler.h"
#include "database.h"
#include <QDebug>

CommandHandler::CommandHandler() {}
QString CommandHandler::handleRequest(const QString& command,
                                      const QStringList& parts,
                                      const QString& clientRole,
                                      int clientUserId)
{
    try {
        // Общие команды
        if (command == "auth") {
            return handleAuth(parts);
        } else if (command == "reg") {
            return handleReg(parts);
        }

        // Проверка аутентификации для остальных команд
        if (clientUserId == -1) {
            return "error&Authentication required\r\n";
        }
        // Обработка новой команды поиска книг
        // ★ Добавляем обработку НОВЫХ команд перед другими
        if (command == "get_book_annotation") {
            return handleGetBookAnnotation(parts);
        } else if (command == "update_book_annotation") {
            if (clientRole != "librarian") return "error&Permission denied\r\n";
            return handleUpdateBookAnnotation(parts);
        }else if (command == "import_books_csv") {
            if (clientRole != "librarian") return "error&Permission denied\r\n";
            if (clientUserId == -1) return "error&Authentication required\r\n"; // Доп. проверка
            return handleImportBooksCsv(parts);
        } else if (command == "export_books_csv") {
            if (clientRole != "librarian") return "error&Permission denied\r\n";
            if (clientUserId == -1) return "error&Authentication required\r\n";
            return handleExportBooksCsv(parts);
        }
        else if (command == "search_books") {
            return handleSearchBooks(parts);
        }else if (command == "get_all_users") {
            // Проверка аутентификации И роли здесь
            if (clientUserId == -1) return "error&Authentication required\r\n";
            if (clientRole != "librarian") return "error&Permission denied\r\n";
            return handleGetAllUsers(parts); // Вызов нового обработчика
        }
        // Команды пользователя
        else if (command == "extend_rental") {
            if (parts.size() != 4) return "error&Invalid parameters\r\n";
            int requestedUserId = parts[1].toInt();

            // Пользователь может продлевать только свои аренды
            if (clientRole != "librarian" && requestedUserId != clientUserId) {
                return "error&Access denied\r\n";
            }
            return handleExtendRental(parts);

        } else if (command == "view_book_stats") {
            if (parts.size() != 2) return "error&Invalid parameters\r\n";
            int requestedUserId = parts[1].toInt();

            if (clientRole != "librarian" && requestedUserId != clientUserId) {
                return "error&Access denied\r\n";
            }
            return handleViewBookStats(parts);

        } else if (command == "view_user_info") {
            if (parts.size() != 2) return "error&Invalid parameters\r\n";
            int requestedUserId = parts[1].toInt();

            // Только библиотекари могут смотреть чужие данные
            if (clientRole != "librarian" && requestedUserId != clientUserId) {
                return "error&Access denied\r\n";
            }
            return handleViewUserInfo(parts);

            // Команды библиотекаря
        } else if (command == "assign_book") {
            if (clientRole != "librarian")
                return "error&Permission denied\r\n";
            return handleAssignBook(parts);

        } else if (command == "unassign_book") {
            if (clientRole != "librarian")
                return "error&Permission denied\r\n";
            return handleUnassignBook(parts);

        } else if (command == "view_user_debts") {
            if (clientRole != "librarian")
                return "error&Permission denied\r\n";
            return handleViewUserDebts(parts);

        } else if (command == "get_user_info") {
            if (clientRole != "librarian")
                return "error&Permission denied\r\n";
            return handleGetUserInfo(parts);

        } else if (command == "add_book_to_lib") {
            if (clientRole != "librarian")
                return "error&Permission denied\r\n";
            return handleAddBookToLibrary(parts);

        } else if (command == "update_book_info") {
            if (clientRole != "librarian")
                return "error&Permission denied\r\n";
            return handleUpdateBookInfo(parts);

        } else if (command == "get_all_books") {
            database& db = database::get_instance();
            QStringList books = db.getAllBooks();
            return QString("books_list+&%1\r\n").arg(books.join("|"));

        } else if (command == "get_all_debts") {
            if (clientRole != "librarian")
                return "error&Permission denied\r\n";
            database& db = database::get_instance();
            QStringList debts = db.getAllDebts();
            return QString("all_debts+&%1\r\n").arg(debts.join("|"));
        }

        return "error&Unknown command\r\n";
    }
    catch (const std::exception& e) {
        qCritical() << "Error handling request:" << e.what();
        return "error&Server error\r\n";
    }
}

// Реализация нового обработчика поиска книг
QString CommandHandler::handleSearchBooks(const QStringList& parts) {
    if (parts.size() == 3) { // Ожидаем: search_books & criteria & searchTerm
        QString criteria = parts[1].trimmed();
        QString searchTerm = parts[2].trimmed();

        // Проверка валидности критерия (можно сделать строже)
        if (criteria != "title" && criteria != "author" && criteria != "genre") {
            return "error&Invalid search criteria\r\n";
        }
        if (searchTerm.isEmpty()) {
            return "error&Search term cannot be empty\r\n";
        }


        database& db = database::get_instance();
        QStringList books = db.searchBooks(criteria, searchTerm);

        // Используем тот же формат ответа, что и get_all_books,
        // чтобы клиент мог использовать один и тот же обработчик.
        return QString("books_list+&%1\r\n").arg(books.join("|"));
    }
    qDebug() << "Invalid search request parts:" << parts;
    return "error&Invalid parameters for search\r\n";
}

// User commands
QString CommandHandler::handleExtendRental(const QStringList& parts) {
    if (parts.size() == 4) {
        int user_id = parts[1].toInt();
        int book_id = parts[2].toInt();
        int days = parts[3].toInt();

        qDebug() << "Extend rental request: user" << user_id
                 << "book" << book_id
                 << "days" << days;

        database& db = database::get_instance();
        bool success = db.extendRental(user_id, book_id, days);

        if (success) {
            qDebug() << "Extend successful";
            return QString("extend_rental+&%1&%2\r\n").arg(user_id).arg(book_id);
        }
    }
    qDebug() << "Extend failed - invalid request or DB error";
    return "extend_rental-\r\n";
}

QString CommandHandler::handleViewBookStats(const QStringList& parts) {
    if (parts.size() == 2) {
        int user_id = parts[1].toInt();
        database& db = database::get_instance();
        QString stats = db.getUserBooksStats(user_id);
        if (!stats.isEmpty()) {
            return QString("book_stats+&%1\r\n").arg(stats);
        }
    }
    return "book_stats-\r\n";
}

QString CommandHandler::handleViewUserInfo(const QStringList& parts) {
    if (parts.size() == 2) {
        int user_id = parts[1].toInt();
        database& db = database::get_instance();
        QString info = db.getUserFullInfoById(user_id);
        if (!info.isEmpty()) {
            return QString("user_info+&%1\r\n").arg(info);
            // Формат ответа: login,email,password
        }
    }
    return "user_info-\r\n";
}


// Librarian commands
QString CommandHandler::handleAssignBook(const QStringList& parts) {
    if (parts.size() == 3) {
        int user_id = parts[1].toInt();
        int book_id = parts[2].toInt();
        database& db = database::get_instance();
        if (db.assignBookToUser(user_id, book_id)) {
            return QString("assign_book+&%1&%2\r\n").arg(user_id).arg(book_id);
        }
    }
    return "assign_book-\r\n";
}

QString CommandHandler::handleUnassignBook(const QStringList& parts) {
    if (parts.size() == 3) {
        int user_id = parts[1].toInt();
        int book_id = parts[2].toInt();
        database& db = database::get_instance();
        if (db.unassignBookFromUser(user_id, book_id)) {
            return QString("unassign_book+&%1&%2\r\n").arg(user_id).arg(book_id);
        }
    }
    return "unassign_book-\r\n";
}

QString CommandHandler::handleViewUserDebts(const QStringList& parts) {
    if (parts.size() >= 2) {
        int user_id = parts[1].toInt();
        database& db = database::get_instance();
        QStringList debts = db.getUserDebts(user_id); // Было QString -> QStringList

        if (debts.isEmpty()) {
            return "user_rentals+&No active rentals\r\n";
        } else {
            return QString("user_rentals+&%1\r\n").arg(debts.join("|"));
        }
    }
    return "user_rentals-\r\n";
}

QString CommandHandler::handleGetUserInfo(const QStringList& parts) {
    if (parts.size() == 2) {
        QString login = parts[1].trimmed();
        database& db = database::get_instance();
        QString info = db.getUserPublicInfo(login);
        if (!info.isEmpty()) {
            return QString("public_user_info+&%1\r\n").arg(info);
        }
    }
    return "public_user_info-\r\n";
}

QString CommandHandler::handleAddBookToLibrary(const QStringList& parts) {
    if (parts.size() == 5) {
        QString title = parts[1].trimmed();
        QString author = parts[2].trimmed();
        QString year = parts[3].trimmed();
        QString genre = parts[4].trimmed();
        database& db = database::get_instance();
        if (db.addBookToLibrary(title, author, year, genre)) {
            return QString("add_book_lib+&%1\r\n").arg(title);
        }
    }
    return "add_book_lib-\r\n";
}

QString CommandHandler::handleUpdateBookInfo(const QStringList& parts) {
    if (parts.size() == 6) {
        int book_id = parts[1].toInt();
        QString title = parts[2].trimmed();
        QString author = parts[3].trimmed();
        QString year = parts[4].trimmed();
        QString genre = parts[5].trimmed();
        database& db = database::get_instance();
        if (db.updateBookInfo(book_id, title, author, year, genre)) {
            return QString("update_book+&%1\r\n").arg(book_id);
        }
    }
    return "update_book-\r\n";
}

// Common commands
QString CommandHandler::handleAuth(const QStringList& parts) {
    if (parts.size() == 3) {
        QString login = parts[1].trimmed();
        QString password = parts[2].trimmed();
        database& db = database::get_instance();

        if (db.authenticateUser(login, password)) {
            QString role = db.getUserRole(login);
            int user_id = db.getUserID(login);
            return QString("auth+&%1&%2&%3\r\n")
                .arg(login)
                .arg(role)
                .arg(user_id);
        }
    }
    return "auth-\r\n";
}

QString CommandHandler::handleReg(const QStringList& parts) {
    if (parts.size() == 5) {
        QString login = parts[1].trimmed();
        QString password = parts[2].trimmed();
        QString email = parts[3].trimmed();
        QString role = parts[4].trimmed();
        database& db = database::get_instance();
        if (db.registerUser(login, password, email, role)) {
            return QString("reg+&%1\r\n").arg(login);
        }
    }
    return "reg-\r\n";
}

// Реализация обработчика получения аннотации
QString CommandHandler::handleGetBookAnnotation(const QStringList& parts) {
    if (parts.size() == 2) { // Ожидаем: get_book_annotation & book_id
        bool ok;
        int book_id = parts[1].toInt(&ok);
        if (!ok || book_id <= 0) {
            return "error&Invalid book ID format\r\n";
        }

        database& db = database::get_instance();
        QString title_annotation = db.getBookAnnotation(book_id);

        if (!title_annotation.isNull()) {
            QStringList dataParts = title_annotation.split(":::");
            QString title = dataParts.value(0);
            QString annotation = dataParts.value(1);
            // ★ ВОЗВРАЩАЕМ И ID КНИГИ ★
            return QString("annotation+&%1&%2&%3\r\n").arg(book_id).arg(title).arg(annotation);
        } else {
            return "annotation-&Book not found\r\n";
        }
    }
    return "error&Invalid parameters for get_book_annotation\r\n";
}

// Реализация обработчика обновления аннотации
QString CommandHandler::handleUpdateBookAnnotation(const QStringList& parts) {
    if (parts.size() == 3) { // Ожидаем: update_book_annotation & book_id & annotation_text
        bool ok;
        int book_id = parts[1].toInt(&ok);
        if (!ok || book_id <= 0) {
            return "error&Invalid book ID format\r\n";
        }
        QString annotation = parts[2]; // Аннотацию не trim-мим

        database& db = database::get_instance();
        if (db.updateBookAnnotation(book_id, annotation)) {
            return QString("update_annotation+&%1\r\n").arg(book_id); // Ответ: update_annotation+&book_id
        } else {
            // Пытаемся определить причину ошибки
            if (!db.userExists(book_id)) { // Это плохая проверка, нужно проверять книги! Переделать.
                // Правильно: Добавить метод db.bookExists(book_id)
                return "update_annotation-&Book not found\r\n";
            } else {
                return "update_annotation-&Failed to update annotation in DB\r\n";
            }
        }
    }
    return "error&Invalid parameters for update_book_annotation\r\n";
}

// Реализация обработчика получения списка пользователей ★
QString CommandHandler::handleGetAllUsers(const QStringList& parts) {
    // parts здесь не используется, но сигнатура стандартная
    Q_UNUSED(parts);

    database& db = database::get_instance();
    QStringList users = db.getAllUsers();

    // Формат ответа: all_users+&id1,login1|id2,login2|...
    return QString("all_users+&%1\r\n").arg(users.join("|"));
}

QString escapeCsvField(const QString& field) {
    // Если поле содержит запятую, кавычку или символ новой строки, заключаем в кавычки
    if (field.contains(',') || field.contains('"') || field.contains('\n') || field.contains('\r')) {
        QString escapedField = field;
        // Удваиваем внутренние кавычки
        escapedField.replace("\"", "\"\"");
        return "\"" + escapedField + "\"";
    }
    // Иначе возвращаем поле как есть
    return field;
}

// Обработчик ЭКСПОРТА книг в CSV
QString CommandHandler::handleExportBooksCsv(const QStringList& parts) {
    Q_UNUSED(parts); // Параметры пока не используются
    qDebug() << "Handling export_books_csv request";

    database& db = database::get_instance();
    // Получаем данные всех книг (метод должен возвращать и аннотацию!)
    QStringList booksData = db.getAllBooks();

    QString csvContent = "Title,Author,Year,Genre,Annotation\r\n"; // Заголовок CSV

    for (const QString& bookEntry : booksData) {
        QStringList bookParts = bookEntry.split(',');
        if (bookParts.size() < 7) { // Ожидаем ID, Title, Author, Year, Genre, Available, Annotation
            qWarning() << "Skipping invalid book entry during CSV export:" << bookEntry;
            continue;
        }

        // Экранируем поля перед добавлением
        QString title = escapeCsvField(bookParts.value(1));
        QString author = escapeCsvField(bookParts.value(2));
        QString year = escapeCsvField(bookParts.value(3));
        QString genre = escapeCsvField(bookParts.value(4));
        QString annotation = escapeCsvField(bookParts.value(6)); // Аннотация - 7-й элемент (индекс 6)

        csvContent += QString("%1,%2,%3,%4,%5\r\n")
                          .arg(title, author, year, genre, annotation);
    }

    // Формат ответа: export_books+&<csv_содержимое>
    // Важно: убедиться, что сам csvContent не содержит '&', иначе разбор на клиенте сломается!
    // Если '&' может быть в данных, нужно менять разделитель команд или использовать base64.
    // Пока предполагаем, что '&' в данных нет.
    return QString("export_books+&%1\r\n").arg(csvContent);
}


// Обработчик ИМПОРТА книг из CSV
QString CommandHandler::handleImportBooksCsv(const QStringList& parts) {
    qDebug() << "Handling import_books_csv request";
    if (parts.size() < 2) {
        return "error&No CSV data provided for import\r\n";
    }

    // Объединяем все части, начиная со второй, на случай если данные содержали '&'
    // (Хотя лучше было бы передавать данные в Base64 или через POST, но для простоты...)
    QString csvData = parts.mid(1).join('&'); // <- Не очень надежно, если & есть в данных!

    database& db = database::get_instance();
    QStringList lines = csvData.split('\n', Qt::SkipEmptyParts); // Разделяем на строки, пропускаем пустые

    if (lines.size() <= 1) { // Должен быть хотя бы заголовок и одна строка данных
        return "error&CSV data is empty or contains only header\r\n";
    }

    // Проверяем заголовок (опционально, но полезно)
    QString header = lines.first().trimmed().toLower();
    if (!header.startsWith("title,author,year,genre")) {
        qWarning() << "CSV Import: Invalid or missing header. Expected 'Title,Author,Year,Genre,...'. Got:" << lines.first().trimmed();
        // Можно вернуть ошибку, но попробуем обработать без заголовка
        // return "error&Invalid CSV header\r\n";
        // lines.removeFirst(); // Если решаем НЕ ТРЕБОВАТЬ заголовок
    } else {
        lines.removeFirst(); // Удаляем заголовок из списка для обработки
    }


    int successCount = 0;
    int failCount = 0;
    QStringList errorDetails;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;

        // Очень простой парсинг CSV, НЕ учитывает кавычки и запятые внутри полей!
        // Для реального приложения нужна библиотека или более сложный парсер.
        QStringList fields = line.split(',');

        // Ожидаем минимум 4 поля (Title, Author, Year, Genre). Аннотация опциональна.
        if (fields.size() < 4) {
            failCount++;
            errorDetails << QString("Line %1: Invalid column count (%2, expected at least 4)").arg(i + 2).arg(fields.size());
            continue;
        }

        QString title = fields.value(0).trimmed();
        QString author = fields.value(1).trimmed();
        QString year = fields.value(2).trimmed();
        QString genre = fields.value(3).trimmed();
        QString annotation = (fields.size() > 4) ? fields.value(4).trimmed() : ""; // Аннотация опциональна

        // Простая валидация
        if (title.isEmpty() || author.isEmpty() || year.isEmpty() || genre.isEmpty()) {
            failCount++;
            errorDetails << QString("Line %1: Missing required field(s) (Title, Author, Year, Genre)").arg(i + 2);
            continue;
        }
        bool yearOk; year.toInt(&yearOk);
        if (!yearOk) {
            failCount++;
            errorDetails << QString("Line %1: Invalid year format (%2)").arg(i + 2).arg(year);
            continue;
        }

        // Добавляем книгу в БД
        if (db.addBookToLibrary(title, author, year, genre/*, annotation*/)) { // Нужно передать и аннотацию, если addBookToLibrary ее поддерживает
            // Нужно модифицировать addBookToLibrary для приёма аннотации!
            // Пока передаем без неё.
            successCount++;
        } else {
            failCount++;
            // Можно попытаться получить более детальную ошибку из БД, но для простоты:
            errorDetails << QString("Line %1: Failed to add book to DB (maybe duplicate?) Title: %2").arg(i + 2).arg(title);
        }
    }

    // Формируем ответ-сводку
    QString summary = QString("Import complete. Total lines processed: %1. Successful: %2. Failed: %3.")
                          .arg(lines.size()).arg(successCount).arg(failCount);
    if (!errorDetails.isEmpty()) {
        summary += "\nErrors:\n" + errorDetails.join("\n");
    }

    if (failCount > 0) {
        // Возвращаем частичный успех или ошибку? Давайте вернем "+", но с деталями
        return QString("import_books+&%1\r\n").arg(summary);
    } else {
        return QString("import_books+&%1\r\n").arg(summary);
    }
}
