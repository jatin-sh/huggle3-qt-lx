//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.

#ifndef CORE_H
#define CORE_H

#include <iostream>
#include <QApplication>
#include <QNetworkAccessManager>
#include <QList>
#include <QString>
#include <QPluginLoader>
#include <QFile>
#include <QMap>
#include <QtXml>
#include <QMessageBox>
#include "login.h"
#include "configuration.h"
#include "query.h"
#include "wikiedit.h"
#include "mainwindow.h"
#include "message.h"
#include "iextension.h"
#include "hugglequeuefilter.h"
#include "editquery.h"
#include "history.h"
#include "apiquery.h"
#include "exceptionwindow.h"

#ifdef PYTHONENGINE
#include "pythonengine.h"
#endif

namespace Huggle
{
    // Predeclaring some types
    class Login;
    class Query;
    class ApiQuery;
    class MainWindow;
    class HuggleFeed;
    class EditQuery;
    class ProcessorThread;
    class HuggleQueueFilter;
    class WikiSite;
    class WikiPage;
    class WikiUser;
    class WikiEdit;
    class Message;
    class iExtension;

    /*!
     * \brief This is a workaround that allow us to use sleep
     */
    class Sleeper : public QThread
    {
    public:
        static void usleep(unsigned long usecs){QThread::usleep(usecs);}
        static void msleep(unsigned long msecs){QThread::msleep(msecs);}
        static void sleep(unsigned long secs){QThread::sleep(secs);}
    };

    /*!
     * \brief The Language class
     */
    class Language
    {
    public:
        //! Creates new instance of language
        //! param name Name of language
        Language(QString name);
        //! This is a short language name which is used by system
        QString LanguageName;
        //! Long identifier of language that is seen by user
        QString LanguageID;
        QMap<QString, QString> Messages;
    };

    /*!
     * \brief Miscelanceous system functions, all of these functions are static
     *
     * Making any instance of this class is nonsense don't do it :D
     */
    class Core
    {
    public:
        // Global variables
        static QDateTime StartupTime;
        //! Pointer to main
        static MainWindow *Main;
        //! Login form
        static Login *f_Login;
        //! This string contains a html header
        static QString HtmlHeader;
        //! This string contains a html footer
        static QString HtmlFooter;
        //! Pointer to primary feed provider
        static HuggleFeed *PrimaryFeedProvider;
        //! Pointer to secondary feed provider
        static HuggleFeed *SecondaryFeedProvider;
        //! List of all running queries
        static QList<Query*> RunningQueries;
        //! This is basically a list of edits we went through, that
        //! can be deleted from memory anytime we want
        static QList<WikiEdit*> ProcessedEdits;
        //! This is a list of all edits that are being processed by some way
        //! whole list needs to be checked and probed everytime once a while
        static QList<WikiEdit*> ProcessingEdits;
        //! This is a post-processor for edits
        static ProcessorThread * Processor;
        //! List of all messages that are being sent
        static QList<Message*> Messages;
        //! Pending changes
        static QList<EditQuery*> PendingMods;
        //! List of extensions loaded in huggle
        static QList<iExtension*> Extensions;
        static QList<HuggleQueueFilter *> FilterDB;
        //! Languages D:
        static QList<Language*> LocalizationData;
        //! Pointer to AIV page
        static WikiPage * AIVP;
        //! Change this to false when you want to terminate all threads properly (you will need to wait few ms)
        static bool Running;

    #ifdef PYTHONENGINE
        static PythonEngine *Python;
    #endif

        //! Function which is called as one of first when huggle is loaded
        static void Init();
        //! Write text to terminal as well as ring log
        /*!
         * \param Message Message to log
         */
        static void Log(QString Message);
        //! Load extensions (libraries as well as python)
        static void ExtensionLoad();
        /*!
         * \brief VersionRead - read the version from embedded git file
         *
         * This function may be called also from terminal parser
         */
        static void VersionRead();
        //! This log is only shown if verbosity is same or larger than requested verbosity
        static void DebugLog(QString Message, unsigned int Verbosity = 1);
        //! Helper function that will return URL of project in question
        /*!
         * \param Project Site
         * \return String with url
         */
        static QString GetProjectURL(WikiSite Project);
        //! Return a full url like http://en.wikipedia.org/wiki/
        static QString GetProjectWikiURL(WikiSite Project);
        //! Return a script url like http://en.wikipedia.org/w/
        static QString GetProjectScriptURL(WikiSite Project);
        //! Return a base url of current project
        static QString GetProjectURL();
        //! Return a full url like http://en.wikipedia.org/wiki/
        static QString GetProjectWikiURL();
        //! Return a script url like http://en.wikipedia.org/w/
        static QString GetProjectScriptURL();
        static void ProcessEdit(WikiEdit *e);
        //! Terminate the process, call this after you release all resources and finish all queries
        static void Shutdown();
        //! Return a ring log represented as 1 huge string
        static QString RingLogToText();
        /*!
         * \brief Return a ring log as qstring list
         * \return QStringList
         */
        static QStringList RingLogToQStringList();
        static void InsertToRingLog(QString text);
        //! Display a message box telling user that function is not allowed during developer mode
        static void DeveloperError();
        //! Save the local configuration to file
        static void SaveConfig();
        //! Load the local configuration from disk
        static void LoadConfig();
        //! Check the edit summary and similar in order to
        //! determine several edit attributes etc
        static void PreProcessEdit(WikiEdit *_e);
        //! Perform more expensive tasks to finalize
        //! edit processing
        static void PostProcessEdit(WikiEdit *_e);
        //! Check if all running queries are finished and if so it removes them from list
        static void CheckQueries();
        //! Check if we can revert this edit
        static bool PreflightCheck(WikiEdit *_e);
        /*!
         * \brief RevertEdit Reverts the edit
         * \param _e Pointer to edit that needs to be reverted
         * \param summary Summary to use if this is empty the default revert summary is used
         * \param minor If revert should be considered as minor edit
         * \param rollback If rollback feature should be used
         * \param keep Whether the query produced by this function should not be automatically deleted
         * \return Pointer to api query that executes this revert
         */
        static ApiQuery *RevertEdit(WikiEdit* _e, QString summary = "", bool minor = false, bool rollback = true, bool keep = false);
        static QString GetCustomRevertStatus(QString RevertData);
        //! Parse all information from global config on meta
        static bool ParseGlobalConfig(QString config);
        //! Parse all information from local config, this function is used in login
        static bool ParseLocalConfig(QString config);
        static bool ParseUserConfig(QString config);
        //! Parse a string from configuration which has format used by huggle 2x
        /*!
         * \param key Key
         * \param content Text to parse from
         * \param missing Default value in case this key is missing in text
         * \return Value of key
         */
        static QString ConfigurationParse(QString key, QString content, QString missing = "");
        static void LoadDB();
        static bool SafeBool(QString value, bool defaultvalue = false);
        /*!
         * \brief ConfigurationParse_QL Parses a QStringList of values for a given key
         * \param key Key
         * \param content Text to parse key from
         * \param CS Whether the values are separated by comma
         * \return List of values from text or empty list
         */
        static QStringList ConfigurationParse_QL(QString key, QString content, bool CS = false);
        //! Remove leading and finishing space of string
        static QString Trim(QString text);
        //! Remove edit in proper manner
        static void DeleteEdit(WikiEdit *edit);
        // the mess bellow exist because of a way how huggle config stores the lists
        static QString GetSummaryOfWarningTypeFromWarningKey(QString key);
        static QString GetNameOfWarningTypeFromWarningKey(QString key);
        static QString GetKeyOfWarningTypeFromWarningName(QString id);
        //! Parse a part patterns for score words
        static void ParsePats(QString text);
        static void SaveDefs();
        static QString GetValueFromKey(QString item);
        static QString GetKeyFromValue(QString item);
        static void ParseWords(QString text);
        /*!
         * \brief MessageUser Message user
         * \param user Pointer to user
         * \param message Text of message
         * \param title Title
         * \param summary Summary
         * \param section Whether this message should be created in a new section
         * \param dependency Query that is used as a dependency, if it's not NULL
         * the system will wait for it to finish before the message is sent
         * \return
         */
        static Message *MessageUser(WikiUser *user, QString message, QString title, QString summary, bool section = true, Query *dependency = NULL);
        static void LoadDefs();
        static void FinalizeMessages();
        //! Get a level of warning from talk page
        static int GetLevel(QString page);
        static QString RetrieveTemplateToWarn(QString type);
        static EditQuery *EditPage(WikiPage *page, QString text, QString summary = "Edited using huggle", bool minor = false);
        /*!
         * \brief Insert a query to internal list of running queries, so that they can be watched
         * This will insert it to a process list in main form
         * \param item Query that is about to be inserted to list of running queries
         */
        static void AppendQuery(Query* item);
        /*!
         * \brief InsertConfig
         * \param key Configuration key
         * \param value Value of key
         * \param s Stream writer
         */
        static void InsertConfig(QString key, QString value, QXmlStreamWriter *s);
        static void ExceptionHandler(Exception *exception);
        static Language *MakeLanguage(QString text, QString name);
        static void LocalInit(QString name);
        static QString Localize(QString key);
        static void LoadLocalizations();
        static bool ReportPreFlightCheck();
        static void NormalizeConf();
        //! This function creates a user configuration that is stored on wiki
        static QString MakeLocalUserConfig();
    private:
        //! Ring log is a buffer that contains system messages
        static QStringList RingLog;
    };
}

#endif // CORE_H
