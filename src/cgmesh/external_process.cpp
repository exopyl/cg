#include "external_process.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

namespace cgmesh {
namespace process {

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

#ifdef _WIN32
std::string RandomSuffix ()
{
	static std::atomic<std::uint64_t> counter {0};
	std::random_device rd;
	const std::uint64_t mix = (static_cast<std::uint64_t> (rd ()) << 32)
	                        ^ static_cast<std::uint64_t> (rd ())
	                        ^ static_cast<std::uint64_t> (Clock::now ().time_since_epoch ().count ())
	                        ^ (counter.fetch_add (1) * 0x9E3779B97F4A7C15ull);
	std::ostringstream os;
	os << std::hex << mix;
	return os.str ();
}
#endif

// Lit `file` ; au-dela de maxBytes, n'en garde que la fin, precedee d'une marque.
std::string ReadCapturedOutput (const fs::path& file, std::size_t maxBytes)
{
	std::ifstream in (file, std::ios::binary);
	if (!in)
		return {};
	in.seekg (0, std::ios::end);
	const std::streamoff size = in.tellg ();
	if (size <= 0)
		return {};

	std::string prefix;
	std::streamoff start = 0;
	if (static_cast<std::uint64_t> (size) > maxBytes)
	{
		start  = size - static_cast<std::streamoff> (maxBytes);
		prefix = "[...]\n";
	}
	in.seekg (start, std::ios::beg);
	std::string body (static_cast<std::size_t> (size - start), '\0');
	in.read (&body[0], static_cast<std::streamsize> (body.size ()));
	body.resize (static_cast<std::size_t> (in.gcount ()));
	return prefix + body;
}

bool Expired (const ProcessRequest& request, Clock::time_point start)
{
	return request.timeout.count () > 0 && Clock::now () - start >= request.timeout;
}

// La sortie s'ecrit sur disque : sans borne, un programme bavard remplirait le
// repertoire temporaire.
bool OutputTooLarge (const ProcessRequest& request, const fs::path& outputFile)
{
	if (request.maxCapturedBytes == 0)
		return false;
	std::error_code ec;
	const std::uintmax_t size = fs::file_size (outputFile, ec);
	return !ec && size > request.maxCapturedBytes;
}

#ifdef _WIN32

std::wstring Widen (const std::string& s)
{
	if (s.empty ())
		return {};
	const int n = MultiByteToWideChar (CP_UTF8, 0, s.data (), static_cast<int> (s.size ()), nullptr, 0);
	std::wstring out (static_cast<std::size_t> (n), L'\0');
	MultiByteToWideChar (CP_UTF8, 0, s.data (), static_cast<int> (s.size ()), &out[0], n);
	return out;
}

std::string Narrow (const std::wstring& s)
{
	if (s.empty ())
		return {};
	const int n = WideCharToMultiByte (CP_UTF8, 0, s.data (), static_cast<int> (s.size ()), nullptr, 0, nullptr, nullptr);
	std::string out (static_cast<std::size_t> (n), '\0');
	WideCharToMultiByte (CP_UTF8, 0, s.data (), static_cast<int> (s.size ()), &out[0], n, nullptr, nullptr);
	return out;
}

std::string LastErrorMessage (const char* what)
{
	const DWORD code = GetLastError ();
	return std::string (what) + ": " + std::system_category ().message (static_cast<int> (code))
	     + " (" + std::to_string (code) + ")";
}

class Handle
{
public:
	Handle () = default;
	explicit Handle (HANDLE h) : m_h (h == INVALID_HANDLE_VALUE ? nullptr : h) {}
	~Handle () { reset (); }
	Handle (const Handle&) = delete;
	Handle& operator= (const Handle&) = delete;

	HANDLE get () const { return m_h; }
	explicit operator bool () const { return m_h != nullptr; }
	void reset ()
	{
		if (m_h)
			CloseHandle (m_h);
		m_h = nullptr;
	}

private:
	HANDLE m_h = nullptr;
};

class AttributeList
{
public:
	explicit AttributeList (DWORD count)
	{
		SIZE_T size = 0;
		InitializeProcThreadAttributeList (nullptr, count, 0, &size);
		m_storage.resize (size);
		m_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST> (m_storage.data ());
		if (!InitializeProcThreadAttributeList (m_list, count, 0, &size))
			m_list = nullptr;
	}
	~AttributeList ()
	{
		if (m_list)
			DeleteProcThreadAttributeList (m_list);
	}
	AttributeList (const AttributeList&) = delete;
	AttributeList& operator= (const AttributeList&) = delete;

	LPPROC_THREAD_ATTRIBUTE_LIST get () const { return m_list; }

private:
	std::vector<unsigned char>   m_storage;
	LPPROC_THREAD_ATTRIBUTE_LIST m_list = nullptr;
};

ProcessResult RunPlatform (const ProcessRequest& request, const fs::path& outputFile)
{
	ProcessResult result;

	SECURITY_ATTRIBUTES inheritable {};
	inheritable.nLength        = sizeof (inheritable);
	inheritable.bInheritHandle = TRUE;

	Handle output (CreateFileW (outputFile.c_str (), GENERIC_WRITE,
	                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                            &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr));
	if (!output)
	{
		result.error = LastErrorMessage ("cannot create the output capture file");
		return result;
	}
	Handle input (CreateFileW (L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                           &inheritable, OPEN_EXISTING, 0, nullptr));
	if (!input)
	{
		result.error = LastErrorMessage ("cannot open NUL");
		return result;
	}

	// Seuls ces deux handles passent a l'enfant, meme si d'autres handles
	// heritables existent dans le processus appelant.
	HANDLE inherited[2] = { input.get (), output.get () };
	AttributeList attributes (1);
	if (!attributes.get ()
	    || !UpdateProcThreadAttribute (attributes.get (), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
	                                   inherited, sizeof (inherited), nullptr, nullptr))
	{
		result.error = LastErrorMessage ("cannot restrict inherited handles");
		return result;
	}

	STARTUPINFOEXW startup {};
	startup.StartupInfo.cb          = sizeof (startup);
	startup.StartupInfo.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startup.StartupInfo.wShowWindow = SW_HIDE;
	startup.StartupInfo.hStdInput   = input.get ();
	startup.StartupInfo.hStdOutput  = output.get ();
	startup.StartupInfo.hStdError   = output.get ();
	startup.lpAttributeList         = attributes.get ();

	std::wstring commandLine = QuoteWindowsArgument (request.executable.native ());
	for (const std::string& argument : request.arguments)
		commandLine += L' ' + QuoteWindowsArgument (Widen (argument));

	const std::wstring workingDirectory = request.workingDirectory.native ();

	PROCESS_INFORMATION info {};
	const DWORD flags = CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT
	                  | EXTENDED_STARTUPINFO_PRESENT;
	if (!CreateProcessW (request.executable.c_str (), &commandLine[0], nullptr, nullptr, TRUE, flags,
	                     nullptr, workingDirectory.empty () ? nullptr : workingDirectory.c_str (),
	                     &startup.StartupInfo, &info))
	{
		result.error = LastErrorMessage ("cannot start the process");
		return result;
	}
	Handle processHandle (info.hProcess);
	Handle threadHandle (info.hThread);
	output.reset ();
	input.reset ();

	// Un job tue a sa fermeture tout ce qu'il contient : les descendants du
	// programme ne survivent ni a l'annulation, ni a un retour anticipe.
	Handle job (CreateJobObjectW (nullptr, nullptr));
	if (job)
	{
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
		                                        | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
		if (!SetInformationJobObject (job.get (), JobObjectExtendedLimitInformation, &limits, sizeof (limits))
		    || !AssignProcessToJobObject (job.get (), processHandle.get ()))
			job.reset ();
	}
	ResumeThread (threadHandle.get ());

	// Les descendants tues doivent avoir lache leurs handles avant que le
	// repertoire temporaire ne soit supprime : on attend que le job soit vide.
	auto kill = [&] () {
		if (job)
			TerminateJobObject (job.get (), 1);
		else
			TerminateProcess (processHandle.get (), 1);
		WaitForSingleObject (processHandle.get (), INFINITE);
		for (int i = 0; job && i < 100; ++i)
		{
			JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting {};
			if (!QueryInformationJobObject (job.get (), JobObjectBasicAccountingInformation,
			                                &accounting, sizeof (accounting), nullptr)
			    || accounting.ActiveProcesses == 0)
				break;
			Sleep (20);
		}
	};

	const Clock::time_point start = Clock::now ();
	const DWORD slice = static_cast<DWORD> (request.pollInterval.count () > 0 ? request.pollInterval.count () : 50);
	for (;;)
	{
		const DWORD wait = WaitForSingleObject (processHandle.get (), slice);
		if (wait == WAIT_OBJECT_0)
			break;
		if (wait == WAIT_FAILED)
		{
			kill ();
			result.error = LastErrorMessage ("cannot wait for the process");
			return result;
		}
		if (request.shouldCancel && request.shouldCancel ())
		{
			kill ();
			result.outcome = ProcessOutcome::Cancelled;
			return result;
		}
		if (Expired (request, start))
		{
			kill ();
			result.outcome = ProcessOutcome::TimedOut;
			return result;
		}
		if (OutputTooLarge (request, outputFile))
		{
			kill ();
			result.outcome = ProcessOutcome::OutputLimitExceeded;
			return result;
		}
	}

	DWORD exitCode = 0;
	GetExitCodeProcess (processHandle.get (), &exitCode);
	result.outcome  = ProcessOutcome::Exited;
	result.exitCode = static_cast<int> (exitCode);
	return result;
}

#else // POSIX

void SleepFor (std::chrono::milliseconds duration)
{
	timespec ts {};
	ts.tv_sec  = static_cast<time_t> (duration.count () / 1000);
	ts.tv_nsec = static_cast<long> ((duration.count () % 1000) * 1000000);
	while (nanosleep (&ts, &ts) == -1 && errno == EINTR) {}
}

std::string ErrnoMessage (const char* what, int code)
{
	return std::string (what) + ": " + std::strerror (code);
}

class Fd
{
public:
	Fd () = default;
	explicit Fd (int fd) : m_fd (fd) {}
	~Fd () { reset (); }
	Fd (const Fd&) = delete;
	Fd& operator= (const Fd&) = delete;

	int get () const { return m_fd; }
	void reset ()
	{
		if (m_fd >= 0)
			close (m_fd);
		m_fd = -1;
	}

private:
	int m_fd = -1;
};

// Tube dont les deux extremites portent FD_CLOEXEC des leur creation : un
// fork concurrent ailleurs dans le processus ne peut pas en heriter.
bool MakeControlPipe (int fds[2])
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	if (pipe2 (fds, O_CLOEXEC) == 0)
		return true;
	if (errno != ENOSYS)
		return false;
#endif
	if (pipe (fds) != 0)
		return false;
	for (int i = 0; i < 2; ++i)
	{
		const int flags = fcntl (fds[i], F_GETFD);
		if (flags != -1)
			fcntl (fds[i], F_SETFD, flags | FD_CLOEXEC);
	}
	return true;
}

// Enfant, entre fork et execv : ferme tout descripteur >= 3 sauf `keep`.
// N'appelle que des fonctions async-signal-safe.
void CloseInheritedDescriptors (int keep, long maxFd)
{
#if defined(__linux__) && defined(SYS_close_range)
	bool done = true;
	if (keep > 3 && syscall (SYS_close_range, 3u, static_cast<unsigned> (keep - 1), 0u) != 0)
		done = false;
	if (done && syscall (SYS_close_range, static_cast<unsigned> (keep + 1), ~0u, 0u) == 0)
		return;
#endif
	for (long fd = 3; fd < maxFd; ++fd)
		if (fd != keep)
			close (static_cast<int> (fd));
}

// Code d'echec de l'enfant avant execv, ecrit sur le tube de controle.
struct ChildFailure
{
	int stage;   // 0 : chdir, 1 : execv, 2 : flux standard
	int code;    // errno
};

ProcessResult RunPlatform (const ProcessRequest& request, const fs::path& outputFile)
{
	ProcessResult result;

	Fd output (open (outputFile.c_str (), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600));
	if (output.get () < 0)
	{
		result.error = ErrnoMessage ("cannot create the output capture file", errno);
		return result;
	}

	int controlPipe[2] = { -1, -1 };
	if (!MakeControlPipe (controlPipe))
	{
		result.error = ErrnoMessage ("cannot create the control pipe", errno);
		return result;
	}
	Fd controlRead (controlPipe[0]);
	Fd controlWrite (controlPipe[1]);

	// Tout ce dont l'enfant a besoin est prepare AVANT fork : entre fork et
	// execv, l'allocation est interdite.
	const std::string executable = request.executable.string ();
	const std::string workingDirectory = request.workingDirectory.string ();
	std::vector<std::string> storage;
	storage.reserve (request.arguments.size () + 1);
	storage.push_back (executable);
	storage.insert (storage.end (), request.arguments.begin (), request.arguments.end ());
	std::vector<char*> argv;
	argv.reserve (storage.size () + 1);
	for (std::string& s : storage)
		argv.push_back (&s[0]);
	argv.push_back (nullptr);
	long maxFd = sysconf (_SC_OPEN_MAX);
	if (maxFd < 0)
		maxFd = 1024;

	const pid_t pid = fork ();
	if (pid < 0)
	{
		result.error = ErrnoMessage ("cannot fork", errno);
		return result;
	}
	if (pid == 0)
	{
		setpgid (0, 0);
		ChildFailure failure {2, 0};
		int report = controlWrite.get ();
		auto fail = [&failure, &report] (int stage) {
			failure.stage = stage;
			failure.code  = errno;
			(void) !write (report, &failure, sizeof (failure));
			_exit (127);
		};

		// Les descripteurs utiles sont d'abord deplaces au-dessus de 2 : si
		// l'appelant avait ferme 0, 1 ou 2, ils pourraient y avoir atterri et
		// seraient ecrases par les dup2 qui suivent.
		const int control = fcntl (controlWrite.get (), F_DUPFD_CLOEXEC, 3);
		if (control < 0)
			fail (2);
		report = control;
		const int out = fcntl (output.get (), F_DUPFD_CLOEXEC, 3);
		if (out < 0)
			fail (2);
		int null = open ("/dev/null", O_RDONLY | O_CLOEXEC);
		if (null >= 0 && null < 3)
			null = fcntl (null, F_DUPFD_CLOEXEC, 3);

		if (null >= 0 ? dup2 (null, STDIN_FILENO) < 0 : (close (STDIN_FILENO) != 0 && errno != EBADF))
			fail (2);
		if (dup2 (out, STDOUT_FILENO) < 0 || dup2 (out, STDERR_FILENO) < 0)
			fail (2);
		CloseInheritedDescriptors (control, maxFd);

		if (!workingDirectory.empty () && chdir (workingDirectory.c_str ()) != 0)
			fail (0);
		execv (executable.c_str (), argv.data ());
		fail (1);
	}

	setpgid (pid, pid);
	controlWrite.reset ();
	output.reset ();

	// Le tube se ferme a l'execv reussi (FD_CLOEXEC) : une lecture vide signifie
	// que le programme a bien demarre.
	ChildFailure failure {0, 0};
	ssize_t got = 0;
	do
		got = read (controlRead.get (), &failure, sizeof (failure));
	while (got == -1 && errno == EINTR);
	if (got == static_cast<ssize_t> (sizeof (failure)))
	{
		int status = 0;
		while (waitpid (pid, &status, 0) == -1 && errno == EINTR) {}
		result.error = ErrnoMessage (failure.stage == 0 ? "cannot enter the working directory"
		                           : failure.stage == 1 ? "cannot execute the program"
		                                                : "cannot set up the standard streams",
		                             failure.code);
		return result;
	}

	auto kill_ = [&] () {
		kill (-pid, SIGKILL);
		kill (pid, SIGKILL);
		int status = 0;
		while (waitpid (pid, &status, 0) == -1 && errno == EINTR) {}
	};

	const Clock::time_point start = Clock::now ();
	const std::chrono::milliseconds slice = request.pollInterval.count () > 0 ? request.pollInterval
	                                                                          : std::chrono::milliseconds (50);
	for (;;)
	{
		int status = 0;
		const pid_t done = waitpid (pid, &status, WNOHANG);
		if (done == pid)
		{
			result.outcome  = ProcessOutcome::Exited;
			result.exitCode = WIFEXITED (status) ? WEXITSTATUS (status)
			                : WIFSIGNALED (status) ? 128 + WTERMSIG (status) : -1;
			return result;
		}
		if (done == -1 && errno != EINTR)
		{
			result.error = ErrnoMessage ("cannot wait for the process", errno);
			kill_ ();
			return result;
		}
		if (request.shouldCancel && request.shouldCancel ())
		{
			kill_ ();
			result.outcome = ProcessOutcome::Cancelled;
			return result;
		}
		if (Expired (request, start))
		{
			kill_ ();
			result.outcome = ProcessOutcome::TimedOut;
			return result;
		}
		if (OutputTooLarge (request, outputFile))
		{
			kill_ ();
			result.outcome = ProcessOutcome::OutputLimitExceeded;
			return result;
		}
		SleepFor (slice);
	}
}

#endif

} // namespace

TemporaryDirectory::TemporaryDirectory (const std::string& prefix, const fs::path& root)
{
	const fs::path base = root.empty () ? fs::temp_directory_path () : root;
#ifdef _WIN32
	// %TEMP% est propre a l'utilisateur. CreateDirectory est exclusif : un nom
	// deja pris, fichier, repertoire ou lien, n'est jamais adopte.
	for (int attempt = 0; attempt < 16; ++attempt)
	{
		fs::path candidate = base / ("cg-" + prefix + "-" + RandomSuffix ());
		if (fs::create_directory (candidate))
		{
			m_path = std::move (candidate);
			return;
		}
	}
	throw fs::filesystem_error ("cannot create a unique temporary directory", base,
	                            std::make_error_code (std::errc::file_exists));
#else
	// mkdtemp cree le repertoire en 0700, atomiquement et sous un nom
	// imprevisible : sous un /tmp partage, aucun autre utilisateur ne peut y
	// lire ni y deposer quoi que ce soit, pas meme entre creation et chmod.
	const std::string pattern = (base / ("cg-" + prefix + "-XXXXXX")).string ();
	std::vector<char> buffer (pattern.begin (), pattern.end ());
	buffer.push_back ('\0');
	if (mkdtemp (buffer.data ()) == nullptr)
		throw fs::filesystem_error ("cannot create a unique temporary directory", base,
		                            std::error_code (errno, std::generic_category ()));
	m_path = fs::path (buffer.data ());
#endif
}

TemporaryDirectory::~TemporaryDirectory ()
{
	std::error_code ignored;
	if (!m_path.empty ())
		fs::remove_all (m_path, ignored);
}

const char* ProcessOutcomeName (ProcessOutcome outcome)
{
	switch (outcome)
	{
		case ProcessOutcome::Exited:       return "exited";
		case ProcessOutcome::LaunchFailed: return "launch failed";
		case ProcessOutcome::TimedOut:     return "timed out";
		case ProcessOutcome::Cancelled:    return "cancelled";
		case ProcessOutcome::OutputLimitExceeded: return "output limit exceeded";
	}
	return "unknown";
}

ProcessResult RunProcess (const ProcessRequest& request)
{
	ProcessResult result;

	std::error_code ec;
	if (request.executable.empty () || !fs::is_regular_file (request.executable, ec))
	{
		result.error = "program not found: " + request.executable.u8string ();
		return result;
	}
	if (!request.workingDirectory.empty () && !fs::is_directory (request.workingDirectory, ec))
	{
		result.error = "working directory not found: " + request.workingDirectory.u8string ();
		return result;
	}

	try
	{
		TemporaryDirectory scratch ("process", request.temporaryRoot);
		const fs::path outputFile = scratch.path () / "output.log";
		result = RunPlatform (request, outputFile);
		result.output = ReadCapturedOutput (outputFile, request.maxOutputBytes);
	}
	catch (const std::exception& e)
	{
		result = ProcessResult {};
		result.error = e.what ();
	}
	return result;
}

std::string GetEnvironmentVariableUtf8 (const char* name)
{
#ifdef _WIN32
	const std::wstring wname = Widen (name);
	const DWORD n = GetEnvironmentVariableW (wname.c_str (), nullptr, 0);
	if (n == 0)
		return {};
	std::wstring value (n, L'\0');
	const DWORD written = GetEnvironmentVariableW (wname.c_str (), &value[0], n);
	value.resize (written);
	return Narrow (value);
#else
	const char* value = std::getenv (name);
	return value ? std::string (value) : std::string ();
#endif
}

fs::path FindInSystemPath (const std::string& name)
{
	const std::string path = GetEnvironmentVariableUtf8 ("PATH");
#ifdef _WIN32
	const char separator = ';';
#else
	const char separator = ':';
#endif
	std::size_t begin = 0;
	while (begin <= path.size ())
	{
		std::size_t end = path.find (separator, begin);
		if (end == std::string::npos)
			end = path.size ();
		std::string dir = path.substr (begin, end - begin);
		if (dir.size () >= 2 && dir.front () == '"' && dir.back () == '"')
			dir = dir.substr (1, dir.size () - 2);
		if (!dir.empty ())
		{
			try
			{
				std::error_code ec;
				const fs::path candidate = fs::u8path (dir) / fs::u8path (name);
				if (fs::is_regular_file (candidate, ec))
					return candidate;
			}
			catch (const std::exception&)
			{
				// entree du PATH non convertible : ignoree, comme un repertoire absent
			}
		}
		begin = end + 1;
	}
	return {};
}

#ifdef _WIN32
std::wstring QuoteWindowsArgument (const std::wstring& argument)
{
	if (!argument.empty () && argument.find_first_of (L" \t\n\v\"") == std::wstring::npos)
		return argument;

	std::wstring out (1, L'"');
	for (auto it = argument.begin (); ; ++it)
	{
		std::size_t backslashes = 0;
		while (it != argument.end () && *it == L'\\')
		{
			++it;
			++backslashes;
		}
		if (it == argument.end ())
		{
			out.append (backslashes * 2, L'\\');
			break;
		}
		if (*it == L'"')
			out.append (backslashes * 2 + 1, L'\\');
		else
			out.append (backslashes, L'\\');
		out.push_back (*it);
	}
	out.push_back (L'"');
	return out;
}
#endif

} // namespace process
} // namespace cgmesh
