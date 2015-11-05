/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifndef SCANNER_H
#define SCANNER_H

#include <deque>
#include <time.h>
#include "DownloadInfo.h"
#include "Thread.h"
#include "Service.h"

class Scanner : public Service
{
public:
	enum EAddStatus
	{
		asSkipped,
		asSuccess,
		asFailed
	};

private:
	class FileData
	{
	private:
		char*			m_filename;
		long long		m_size;
		time_t			m_lastChange;

	public:
						FileData(const char* filename);
						~FileData();
		const char*		GetFilename() { return m_filename; }
		long long		GetSize() { return m_size; }
		void			SetSize(long long size) { m_size = size; }
		time_t			GetLastChange() { return m_lastChange; }
		void			SetLastChange(time_t lastChange) { m_lastChange = lastChange; }
	};

	typedef std::deque<FileData*>		FileList;

	class QueueData
	{
	private:
		char*				m_filename;
		char*				m_nzbName;
		char*				m_category;
		int					m_priority;
		char*				m_dupeKey;
		int					m_dupeScore;
		EDupeMode			m_dupeMode;
		NzbParameterList	m_parameters;
		bool				m_addTop;
		bool				m_addPaused;
		NzbInfo*			m_urlInfo;
		EAddStatus*			m_addStatus;
		int*				m_nzbId;

	public:
							QueueData(const char* filename, const char* nzbName, const char* category,
								int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
								NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo,
								EAddStatus* addStatus, int* nzbId);
							~QueueData();
		const char*			GetFilename() { return m_filename; }
		const char*			GetNzbName() { return m_nzbName; }
		const char*			GetCategory() { return m_category; }
		int					GetPriority() { return m_priority; }
		const char*			GetDupeKey() { return m_dupeKey; }
		int					GetDupeScore() { return m_dupeScore; }
		EDupeMode			GetDupeMode() { return m_dupeMode; }
		NzbParameterList*	GetParameters() { return &m_parameters; }
		bool				GetAddTop() { return m_addTop; }
		bool				GetAddPaused() { return m_addPaused; }
		NzbInfo*			GetUrlInfo() { return m_urlInfo; }
		void				SetAddStatus(EAddStatus addStatus);
		void				SetNzbId(int nzbId);
	};

	typedef std::deque<QueueData*>		QueueList;

	bool				m_requestedNzbDirScan;
	int					m_nzbDirInterval;
	bool				m_scanScript;
	int					m_pass;
	FileList			m_fileList;
	QueueList			m_queueList;
	bool				m_scanning;
	Mutex				m_scanMutex;

	void				CheckIncomingNzbs(const char* directory, const char* category, bool checkStat);
	bool				AddFileToQueue(const char* filename, const char* nzbName, const char* category,
							int priority, const char* dupeKey, int dupeScore, EDupeMode dupeMode,
							NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo, int* nzbId);
	void				ProcessIncomingFile(const char* directory, const char* baseFilename,
							const char* fullFilename, const char* category);
	bool				CanProcessFile(const char* fullFilename, bool checkStat);
	void				DropOldFiles();
	void				ClearQueueList();

protected:
	virtual int			ServiceInterval() { return 200; }
	virtual void		ServiceWork();

public:
						Scanner();
						~Scanner();
	void				InitOptions();
	void				ScanNzbDir(bool syncMode);
	EAddStatus			AddExternalFile(const char* nzbName, const char* category, int priority,
							const char* dupeKey, int dupeScore, EDupeMode dupeMode,
							NzbParameterList* parameters, bool addTop, bool addPaused, NzbInfo* urlInfo,
							const char* fileName, const char* buffer, int bufSize, int* nzbId);
	void				InitPPParameters(const char* category, NzbParameterList* parameters, bool reset);
};

extern Scanner* g_Scanner;

#endif
