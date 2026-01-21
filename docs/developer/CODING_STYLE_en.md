# EasyKiConverter Coding Style

This document provides a unified set of coding standards and best practices for developers of the EasyKiConverter project, aiming to ensure code consistency, readability, and maintainability.

This guide is primarily based on the [Qt Coding Style](https://wiki.qt.io/Qt_Coding_Style), combined with the project's own established practices.

## 1. Naming Conventions

### 1.1. General Rules

*   Use clear and descriptive English names.
*   Avoid abbreviations unless they are widely known (e.g., `UI`, `API`).

### 1.2. File Names

*   C++ source and header files should use snake_case, e.g., `export_service_pipeline.cpp` and `export_service_pipeline.h`.
*   QML files should use PascalCase, e.g., `MainWindow.qml`.

### 1.3. Classes, Structs, and Namespaces

*   Use PascalCase.
*   **Examples**: `ExportServicePipeline`, `ComponentData`, `EasyKiConverter`.

### 1.4. Functions and Methods

*   Use camelCase.
*   **Examples**: `executeExportPipeline()`, `handleFetchCompleted()`.

### 1.5. Variables

*   **Local Variables**: Use camelCase.
    *   **Examples**: `int totalCount = 0;`, `QMutexLocker locker(m_mutex);`
*   **Member Variables**:
    *   **Public members**: Use camelCase without any prefix.
    *   **Private/Protected members**: Use the `m_` prefix, followed by camelCase.
    *   **Examples**: `QThreadPool *m_fetchThreadPool;`, `int m_successCount;`
*   **Global Variables**: Avoid global variables whenever possible. If one must be used, it should be prefixed with `g_`.
*   **Constants and Enums**:
    *   **Constants**: Use UPPER_SNAKE_CASE.
        *   **Example**: `const int MAX_THREADS = 8;`
    *   **Enum Names**: Use PascalCase.
    *   **Enum Values**: Use UPPER_SNAKE_CASE.

### 1.6. QML

*   **IDs**: Use camelCase.
*   **Properties**: Use camelCase.

## 2. Formatting

### 2.1. Indentation

*   Use **4 spaces** for indentation. Do not use tabs.

### 2.2. Braces `{}`

*   **Functions, Classes, Namespaces, Enums**: The opening brace `{` goes on a **new line**.
    ```cpp
    class MyClass
    {
    public:
        void myFunction()
        {
            // ...
        }
    };
    ```
*   **Control Statements (if, for, while, switch)**: The opening brace `{` goes at the **end of the current line**.
    ```cpp
    if (condition) {
        // ...
    } else {
        // ...
    }
    ```
*   **Single-Line Statements**: Braces are mandatory even if the body of a control statement is a single line.
    ```cpp
    // Correct
    if (condition) {
        return;
    }

    // Incorrect
    if (condition) return;
    ```

### 2.3. Line Length

*   Try to keep the line length limited to **120 characters**.

### 2.4. Spaces

*   Use a space around binary operators (`+`, `-`, `*`, `/`, `=`, `==`, `!=`, `&&`, etc.).
*   Use a space after commas `,` and semicolons `;` (in for loops).
*   Use a space between a keyword (`if`, `for`, `while`) and the following parenthesis `(`.
*   Do **not** use a space between a function name and its argument list's parenthesis `(`.

## 3. Comments

*   Prefer self-documenting code. Add comments only when the intent of the code is not obvious.
*   Use C++-style `//` for single-line and end-of-line comments.
*   For detailed descriptions of functions, classes, or complex code blocks, use Doxygen-style comments.
    ```cpp
    /**
     * @brief A brief description.
     *
     * A more detailed description goes here.
     * @param param1 Description of parameter 1.
     * @return Description of the return value.
     */
    ```

## 4. Modern C++ Features

### 4.1. Resource Management

*   **Strongly recommend** using the RAII (Resource Acquisition Is Initialization) idiom.
*   **Prefer smart pointers** to manage dynamically allocated memory over raw pointers and manual `new`/`delete`.
    *   Use `std::unique_ptr` or `QScopedPointer` to represent exclusive ownership.
    *   Use `std::shared_ptr` or `QSharedPointer` to represent shared ownership.
*   **Example**:
    ```cpp
    // Recommended
    std::unique_ptr<MyObject> obj = std::make_unique<MyObject>();

    // Not recommended
    MyObject* obj = new MyObject();
    // ...
    delete obj;
    ```

### 4.2. `nullptr`

*   Always use `nullptr`, not `0` or `NULL`, to represent a null pointer.

### 4.3. Lambda Expressions

*   Lambda expressions are encouraged, especially for signal-slot connections and simple asynchronous tasks.
*   Keep lambdas short and easy to understand. If the logic is complex, refactor it into a separate member or static function.

### 4.4. `auto`

*   Use `auto` judiciously. It can improve readability when the variable type is long or clearly inferable from the initialization expression.
*   Do not overuse `auto` to the point where it obscures the code's clarity.

## 5. Qt Specific Practices

### 5.1. QObject Parent-Child Relationship

*   Whenever possible, leverage Qt's parent-child object tree to manage the lifetime of `QObject` derivatives.
*   When creating an instance of a `QObject` derived class, assign a `parent` if possible.
    ```cpp
    // Correct: `service` will be automatically deleted when its parent is destroyed.
    auto service = new MyService(this);
    ```

### 5.2. Signals and Slots

*   **Prefer** the new signal-slot connection syntax (based on function pointers) as it provides compile-time checking.
    ```cpp
    // Recommended
    connect(sender, &Sender::valueChanged, receiver, &Receiver::updateValue);

    // Not recommended (unless necessary)
    connect(sender, SIGNAL(valueChanged(int)), receiver, SLOT(updateValue(int)));
    ```

### 5.3. Strings

*   Use `QString` for UI elements and in parts that require internationalization.
*   Consider using `std::string` when dealing with raw data or interacting with non-Qt libraries, and convert to/from `QString` when needed.

### 5.4. Logging

*   Use Qt's logging framework (`qDebug`, `qInfo`, `qWarning`, `qCritical`).
*   `qDebug` messages are automatically removed in release builds, so they can be used freely for development and debugging.
*   Important errors and warnings should use `qWarning` or `qCritical`.
