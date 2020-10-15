#include <QApplication>
#include <QtWidgets>
#include <QWebEngineView>
#include <iostream>
#include "facebookpage.h"
#include "facebookuser.h"

void showResultsWindow(const QVector<FacebookUser> &friendChains){
    static QTabWidget *resultsWindow = nullptr;
    if(resultsWindow != nullptr){
        delete resultsWindow;
    }

    resultsWindow = new QTabWidget;
    resultsWindow->setWindowIcon(QIcon(":/icon.ico"));
    resultsWindow->setWindowTitle(QObject::tr("Results"));

    int i = 1;
    for(const FacebookUser &chain: friendChains){
        QWidget *optionTab = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout;

        QLabel *label = new QLabel(QObject::tr("Friend chain from you to <a href=\"%1\">%1</a>:").arg(chain.profileUrl()));
        label->setTextFormat(Qt::RichText);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setOpenExternalLinks(true);
        label->setStyleSheet("QLabel{padding:8px}");

        int j = 1;
        for(const FacebookUser &f: chain.friendChain()){
            label->setText(label->text() + QString("<br>%1. <a href=\"%2\">%2</a>").arg(QString::number(j), f.profileUrl()));
            j++;
        }
        label->setText(label->text() + QString("<br>%1. <a href=\"%2\">%2</a>").arg(QString::number(j), chain.profileUrl()));
        layout->addWidget(label);

        QPushButton *saveButton = new QPushButton(QObject::tr("Save results"));
        QObject::connect(saveButton, &QPushButton::pressed, [chain](){
            const QString filePath = QFileDialog::getSaveFileName(resultsWindow, QObject::tr("Save results"), "", "Text document (*.txt)");
            if(filePath.isEmpty()){
                return;
            }
            QFile file(filePath);
            if(!file.open(QIODevice::WriteOnly | QIODevice::Text)){
                QMessageBox::critical(resultsWindow, "", QObject::tr("An error occurred when saving the file."));
                return;
            }
            QTextStream out(&file);
            for(const FacebookUser &f: chain.friendChain()){
                out << f.profileUrl() << "\n";
            }
            out << chain.profileUrl();
            file.close();
        });
        layout->addWidget(saveButton);

        optionTab->setLayout(layout);
        resultsWindow->addTab(optionTab, QObject::tr("Option %1").arg(i));
        i++;
    }

    resultsWindow->show();
}

void findNextFriends(QVector<FacebookUser> *friends, FacebookPage *facebookPage, FacebookUser profileToFind, bool ignoreCache, QRegExp keyword, QWidget *progressWindow, QProgressBar *progressBar, QLabel *progressLabel, QLabel *resultsLabel, QPushButton *resultsButton){
    static int processedFriends = 0;
    static int processedFriendsOfCurrentLevel = 0;
    static int previousLevel = 0;
    static int levelLength = friends->length();
    static QVector<FacebookUser> friendChains;

    if(!progressWindow->isVisible()){
        delete friends;
    }
    else if(processedFriends < friends->length()){
        const FacebookUser &f = (*friends)[processedFriends];
        if(previousLevel < f.level()){
            progressLabel->setText(QObject::tr("Searching for people %n friends away from you...", nullptr, f.level() + 1));
            levelLength = friends->length() - processedFriendsOfCurrentLevel;
            processedFriendsOfCurrentLevel = 0;
            previousLevel = f.level();
        }
        progressBar->setValue(100 * processedFriendsOfCurrentLevel / levelLength);
        f.findFriends(facebookPage, keyword, profileToFind, ignoreCache, [=](const QVector<FacebookUser> &moreFriends){
            std::cout << f.profileUrl().toStdString() << std::endl;
            for(const FacebookUser &newFriend: moreFriends){
                if(newFriend == profileToFind){
                    friendChains.append(newFriend);
                    resultsLabel->setText(QObject::tr("%n friend chains found", nullptr, friendChains.length()));
                    resultsButton->setDisabled(false);
                    resultsButton->disconnect();
                    QObject::connect(resultsButton, &QPushButton::pressed, [=](){
                        showResultsWindow(friendChains);
                    });
                }
                else{
                    friends->append(newFriend);
                }
            }
            findNextFriends(friends, facebookPage, profileToFind, ignoreCache, keyword, progressWindow, progressBar, progressLabel, resultsLabel, resultsButton);
        });
        processedFriends++;
        processedFriendsOfCurrentLevel++;
    }
    else if(!friendChains.isEmpty()){
        QMessageBox::information(progressWindow, "", QObject::tr("Search for friend chains completed."));
        showResultsWindow(friendChains);
        progressWindow->close();
    }
    else{
        QMessageBox::warning(progressWindow, "", QObject::tr("Could not find friend chains from you to <a href=\"%1\">%1</a>. Make sure this is a valid Facebook profile, or try using a less restrictive common keyword.").arg(profileToFind.profileUrl()));
        qApp->quit();
    }
}

void showInitialWindow(FacebookPage *facebookPage){
    QWidget *initialWindow = new QWidget;
    initialWindow->setWindowIcon(QIcon(":/icon.ico"));
    QFormLayout *layout = new QFormLayout;

    QWidget *myProfileWidget = new QWidget;
    QHBoxLayout *myProfileLayout = new QHBoxLayout;
    myProfileLayout->setMargin(0);
    QLabel *myProfileLabel = new QLabel(QString("<a href='%1'>%1</a>").arg(facebookPage->loggedInUser()));
    myProfileLabel->setTextFormat(Qt::RichText);
    myProfileLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    myProfileLabel->setOpenExternalLinks(true);
    myProfileLayout->addWidget(myProfileLabel);
    myProfileWidget->setLayout(myProfileLayout);
    layout->addRow(QObject::tr("Your Facebook profile"), myProfileWidget);

    QPushButton *logOutButton = new QPushButton(QObject::tr("Log out"));
    QObject::connect(logOutButton, &QPushButton::pressed, [=](){
        qApp->setQuitOnLastWindowClosed(false);
        facebookPage->logOut();
        facebookPage->logIn([facebookPage](){
            showInitialWindow(facebookPage);
            qApp->setQuitOnLastWindowClosed(true);
        });
        delete initialWindow;    //Qt will automatically delete all the widgets inside
    });
    myProfileLayout->addWidget(logOutButton);

    const QStringList &args = qApp->arguments();

    QLineEdit *profileToFindInput = new QLineEdit;
    if(args.length() > 1){
        profileToFindInput->setText(args[1]);
    }
    profileToFindInput->setPlaceholderText("https://www.facebook.com/john.doe");
    layout->addRow(QObject::tr("Facebook profile of person to find"), profileToFindInput);

    QLineEdit *commonKeywordInput = new QLineEdit;
    if(args.contains("-k") && args.indexOf("-k") < args.length() - 1){
        commonKeywordInput->setText(args[args.indexOf("-k") + 1]);
    }
    commonKeywordInput->setPlaceholderText(QObject::tr("For example city where you live, school where you study, company where you work..."));
    QCheckBox *useRegexCheckbox = new QCheckBox(QObject::tr("Use regular expressions"));
    useRegexCheckbox->setChecked(args.contains("-r"));
    QHBoxLayout *commonKeywordLayout = new QHBoxLayout;
    commonKeywordLayout->addWidget(commonKeywordInput);
    commonKeywordLayout->addWidget(useRegexCheckbox);
    layout->addRow(QObject::tr("Common keyword (optional, makes searching faster)"), commonKeywordLayout);

    QCheckBox *ignoreCacheCheckox = new QCheckBox(QObject::tr("Ignore cached data"));
    ignoreCacheCheckox->setChecked(args.contains("-i"));
    layout->addRow(ignoreCacheCheckox);

    QPushButton *searchButton = new QPushButton(QObject::tr("Search for common friends"));
    layout->addRow(searchButton);

    QPushButton *aboutButton = new QPushButton(QObject::tr("About CommonFriends"));
    QObject::connect(aboutButton, &QPushButton::pressed, [initialWindow](){
        QMessageBox::about(initialWindow, QObject::tr("About CommonFriends"), "CommonFriends<br><br>" + QObject::tr("Copyright (C) 2020 Gustav Lindberg.") + "<br><br>" + QObject::tr("Icons made by %3 from %1 are licensed by %2.").arg("<a href=\"https://www.flaticon.com/\"> www.flaticon.com</a>", "<a href=\"http://creativecommons.org/licenses/by/3.0/\">CC 3.0 BY</a>", "<a href=\"https://www.flaticon.com/authors/freepik\">Freepik</a>"));
    });
    layout->addRow(aboutButton);

    initialWindow->setLayout(layout);

    const auto startSearch = [=](){
        const QString profileToFind = profileToFindInput->text().replace(QRegExp("(^https?://(?:www\\.)?facebook\\.com/)|(/$)"), "");
        const bool ignoreCache = ignoreCacheCheckox->isChecked();
        if(!profileToFind.contains(QRegExp("^([0-9a-zA-Z\\.]+|profile\\.php\\?id=[0-9]+)$"))){
            QMessageBox::warning(initialWindow, "", QObject::tr("Please enter a valid Facebook profile for the person to find."));
            initialWindow->show();
            return;
        }
        const QRegExp keyword(useRegexCheckbox->isChecked() ? commonKeywordInput->text() : QRegExp::escape(commonKeywordInput->text()), Qt::CaseInsensitive);
        if(!keyword.isValid()){
            QMessageBox::warning(initialWindow, "", QObject::tr("Please enter a valid regular expression, or uncheck the \"Use regular expressions\" box."));
            initialWindow->show();
            return;
        }
        delete initialWindow;    //Qt will automatically delete all the widgets inside

        QWidget *window = new QWidget;
        window->setWindowIcon(QIcon(":/icon.ico"));
        QVBoxLayout *layout = new QVBoxLayout;
        layout->setMargin(10);

        QLabel *progressLabel = new QLabel(QObject::tr("Searching for your friends..."));
        progressLabel->setFixedHeight(30);
        layout->addWidget(progressLabel);

        QWidget *resultsRow = new QWidget;
        resultsRow->setFixedHeight(50);
        QFormLayout *resultsLayout = new QFormLayout;
        resultsLayout->setMargin(0);
        QLabel *resultsLabel = new QLabel(QObject::tr("No results found yet"));
        resultsLabel->setStyleSheet("QLabel{padding-right:8px}");
        QPushButton *resultsButton = new QPushButton(QObject::tr("Show results"));
        resultsButton->setDisabled(true);
        resultsRow->setLayout(resultsLayout);
        resultsLayout->addRow(resultsLabel, resultsButton);
        layout->addWidget(resultsRow);

        QProgressBar *progressBar = new QProgressBar;
        progressBar->setFixedHeight(30);
        layout->addWidget(progressBar);

        QWidget *webViewContainer = new QWidget;
        QVBoxLayout *webViewLayout = new QVBoxLayout;
        webViewLayout->setMargin(100);
        webViewLayout->addWidget(&facebookPage->_webView);    //For some reason if Facebook doesn't think the web engine widget is visible it will only show 8 friends
        facebookPage->_webView.setFixedSize(400, 400);    //So that making the window smaller doesn't make the web engine so small that Facebook knows it's not visible
        webViewContainer->setLayout(webViewLayout);
        layout->addWidget(webViewContainer);

        window->setFixedSize(400, 150);    //Fix the height so that the web engine widget isn't visible even though Facebook thinks it is
        window->setLayout(layout);
        window->show();

        QVector<FacebookUser> *friends = new QVector<FacebookUser>({FacebookUser(facebookPage->loggedInUser(), nullptr)});

        findNextFriends(friends, facebookPage, FacebookUser(profileToFind, nullptr), ignoreCache, keyword, window, progressBar, progressLabel, resultsLabel, resultsButton);
    };
    QObject::connect(searchButton, &QPushButton::pressed, startSearch);

    if(args.length() == 1 || args.contains("-w")){
        initialWindow->show();
    }
    else{
        startSearch();
    }
}

int main(int argc, char **argv){
    QApplication app(argc, argv);

    FacebookUser::loadCachedFriendsLists();
    FacebookPage facebookPage;
    facebookPage.logIn([&facebookPage](){
        showInitialWindow(&facebookPage);
    });

    return app.exec();
}
