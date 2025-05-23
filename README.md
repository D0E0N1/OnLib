# OnLib

**OnLib** - это клиент-серверное приложение, представляющее собой систему управления цифровой библиотекой, разработанное с использованием `C++` и фреймворка `Qt`. Система позволяет моделировать основные процессы взаимодействия читателей и библиотекарей с книжным фондом.

## Основные возможности:

### Управление пользователями:
*   Поддержка двух ролей: **Клиент** (читатель) и **Библиотекарь**.
*   **Регистрация** новых пользователей с валидацией вводимых данных (логин, пароль, email).
*   **Аутентификация** существующих пользователей.
*   **Очистка** полей форм при выходе из системы.

### Каталог книг:
*   **Просмотр** полного каталога книг для клиентов и библиотекарей.
*   Отображение основной информации: название, автор, год, жанр, **статус доступности**.
*   **Поиск** книг по названию, автору или жанру.
*   Кнопка **"Показать все"** для сброса фильтров поиска.

### Управление фондом (для Библиотекаря):
*   **Добавление** новых книг в каталог.
*   **Редактирование** информации о существующих книгах.
*   **Управление аннотациями** (добавление, редактирование).
*   Пакетный **импорт/экспорт каталога** в формате `CSV`.

### Аренда книг:
*   **Выдача** книг клиентам и фиксация **возврата** (Библиотекарь).
*   Просмотр **текущих аренд** с датами (Клиент).
*   **Продление** срока аренды (Клиент).

### Дополнительно:
*   **Просмотр аннотаций** книг (Клиент).
*   Просмотр **списка зарегистрированных клиентов** (Библиотекарь).
*   **Уведомления** клиентам о приближающемся сроке возврата и о просрочках.
*   Переключение **темы интерфейса** (светлая/темная).

## Технологический стек:

*   **Язык:** `C++` (стандарт `C++11`)
*   **Фреймворк:** `Qt 5/6` (модули `Core`, `GUI`, `Network`, `SQL`, `Widgets`)
*   **База данных:** `SQLite`
*   **Сетевое взаимодействие:** `TCP/IP` (клиент-сервер)
*   **Интерфейс:** `Qt Widgets` с использованием `QPalette` для стилизации.

Проект OnLib представляет собой основу для создания полнофункциональной системы управления библиотекой, демонстрируя работу с базой данных, сетевое взаимодействие и построение графического интерфейса на Qt.
