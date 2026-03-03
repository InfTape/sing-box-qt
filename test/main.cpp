#include <QApplication>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Global stylesheet
    app.setStyleSheet(R"(
        QLineEdit, QPlainTextEdit {
            background-color: #f0f4f8;
            border: 1px solid #d0d5dd;
            border-radius: 10px;
            padding: 8px 12px;
            color: #1a1a2e;
            font-size: 14px;
        }
        QLineEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #5b9bd5;
        }
    )");

    QDialog dialog;
    dialog.setWindowTitle("QPlainTextEdit Placeholder Bug");
    dialog.setMinimumWidth(400);

    QFormLayout* form = new QFormLayout(&dialog);

    // QLineEdit — placeholder works correctly
    QLineEdit* lineEdit = new QLineEdit;
    lineEdit->setPlaceholderText("This placeholder disappears correctly");
    form->addRow("QLineEdit:", lineEdit);

    // QPlainTextEdit — placeholder does NOT disappear when typing
    QPlainTextEdit* plainEdit = new QPlainTextEdit;
    plainEdit->setPlaceholderText("This placeholder stays visible when typing!");
    plainEdit->setFixedHeight(80);
    form->addRow("QPlainTextEdit:", plainEdit);

    dialog.show();
    return app.exec();
}