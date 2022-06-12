#include "Worker.h"
#include <sys/stat.h>
#include "HttpResponseBuilder.h"

#include "CgiWorker.h"
#include <fstream>
#include <sstream>

Worker::Worker() {}
Worker::~Worker() {}

HttpResponse Worker::ProcessRequest(HttpRequest* request, const VirtualServer* virtual_server, const VirtualServer::UriProps* location) {
	if (!location || !virtual_server || !location->IsMethodAllowed(request->GetMethod())) {
		return HttpResponseBuilder::GetInstance().CreateErrorResponse(405, virtual_server);
	}
	if (request->GetPath().find("..") != std::string::npos) {
		return HttpResponseBuilder::GetInstance().CreateErrorResponse(403, virtual_server);
	}

	if (virtual_server->IsCgiPath(request->GetPath()) || location->IsCgiPath(request->GetPath())) {
		std::cout << "Start CGI" << std::endl;
		return ProcessCGIRequest(request, virtual_server, location);
	}

	switch (ToHttpMethod(request->GetMethod()))
	{
	case GET:
		return HttpGet(request, virtual_server, location);
		break;
	case HEAD:
		return HttpHead(request, virtual_server, location);
		break;
	case POST:
		return HttpPost(request, virtual_server, location);
		break;
    case PUT:
        return HttpPost(request, virtual_server, location);
        break;
	default:
		return HttpResponseBuilder::GetInstance().CreateErrorResponse(501, virtual_server);
		break;
	}
}

HttpResponse Worker::ProcessCGIRequest(HttpRequest* request, const VirtualServer* virtual_server, const VirtualServer::UriProps* location) {
	std::string request_address = request->GetPath().substr(location->uri.size());
	std::string cgi_script_path = location->cgi_script;
//	while (request_address.size()) {
//		size_t first_slash = request_address.find('/');
//        if (first_slash == 0) {
//            request_address = request_address.substr(first_slash + 1);
//            continue;
//        }
//		std::string file_or_dir;
//		if (first_slash != std::string::npos) {
//			file_or_dir = request_address.substr(0, first_slash);
//		} else {
//			file_or_dir = request_address;
//		}
//		if (!IsPathExist(cgi_script_path + "/" + file_or_dir)) {
//            break;
//        }
//		request_address = request_address.substr(0, first_slash + 1);
//		cgi_script_path += "/" + file_or_dir;
//        if (IsFileExist(cgi_script_path) || first_slash == std::string::npos) {
//            break;
//        }
//	}
    if (!IsFileExist(cgi_script_path)) {
        return HttpResponseBuilder::GetInstance().CreateErrorResponse(500, virtual_server);
    }
	CgiWorker cgi_worker;
	std::string responce_body = cgi_worker.executeCgi(cgi_script_path, request_address, request);
	int status = 200;
    if (responce_body.find("Status: ") != std::string::npos) {
        std::string status_line = responce_body.substr(8, 3);
        std::istringstream(status_line) >> status;
        responce_body = responce_body.substr(12);
    }
	HttpResponse response = HttpResponseBuilder::GetInstance().CreateResponse(responce_body, status);
	return response;
}

std::string ResolvePagePath(std::string request_address, const VirtualServer* virtual_server, const VirtualServer::UriProps* location) {
    if (request_address == "") {
        request_address = "/";
    }
	std::string path = location->path + "/" + request_address.substr(location->uri.size());
    struct stat s;
    if( stat(path.c_str(), &s) == 0) {
        if (s.st_mode & S_IFDIR) {
            path += "/" + virtual_server->m_StandardRoutes.at(LocationNames::Index);
        }
    }

	return path;
}

bool Worker::IsPathExist(std::string path) {
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

bool Worker::IsDirExist(std::string file_path) {
    struct stat buffer;
    if (!(stat(file_path.c_str(), &buffer) == 0)) {
        return false;
    }
    return buffer.st_mode & S_IFDIR;
}

bool Worker::IsFileExist(std::string file_path) {
    struct stat buffer;
    if (!(stat(file_path.c_str(), &buffer) == 0)) {
        return false;
    }
    return S_ISREG(buffer.st_mode);
}




HttpResponse Worker::HttpGet(HttpRequest* request, const VirtualServer* virtual_server, const VirtualServer::UriProps* location) {
	std::string file_path = ResolvePagePath(request->GetPath(), virtual_server, location);

	if (!IsFileExist(file_path)) {
		return HttpResponseBuilder::GetInstance().CreateErrorResponse(404, virtual_server);
	}

	std::string file_content = ReadFile(file_path);

	HttpResponse response = HttpResponseBuilder::GetInstance().CreateResponse(file_content, 200);
	// response.SetHeader("тип файла", "текст");

	return response;
}

HttpResponse Worker::HttpHead(HttpRequest* request, const VirtualServer* virtual_server, const VirtualServer::UriProps* location) {
	HttpResponse response = HttpGet(request, virtual_server, location);
	response.body = "";
	return response;
}

std::string ExtractFileName(HttpRequest* request) {
    if (!request->count("Content-Disposition")) {
        return "";
    }
    std::string filename_part = request->at("Content-Disposition");
    if (filename_part.find("filename") == std::string::npos) {
        return "";
    }
    filename_part = filename_part.substr(filename_part.find("filename") + 9);
    filename_part = filename_part.substr(filename_part.find("\"") + 1);
    filename_part = filename_part.substr(0, filename_part.find("\""));
    std::cout << filename_part << std::endl;
    return filename_part;
}


HttpResponse Worker::HttpPost(HttpRequest* request, const VirtualServer* virtual_server, const VirtualServer::UriProps* location) {
	// POST метод можно обработать только с помощью CGI
    std::string file = ExtractFileName(request);
    std::string file_path;
    std::string request_path = request->GetPath();
    if (location->uri.size() > 1) {
        request_path = request_path.substr(location->uri.size());
    }
    std::string dir = location->path + request_path;
    if (!ends_with(request_path, "/")) {
        size_t sep = request_path.rfind("/");
        file_path = location->path + request_path;
        dir = location->path + request_path.substr(0, sep);
    } else if (file.size()) {
        file_path = location->path + request_path + "/" + file;
    } else {
        file_path = location->path + request_path + "/ " + "loaded_file.tmp";
    }
    if (!IsDirExist(dir)) {
        return HttpResponseBuilder::GetInstance().CreateErrorResponse(404, virtual_server);
    }

    std::ofstream file_stram;
    file_stram.open(file_path);
    file_stram << std::string(request->GetBody().begin(), request->GetBody().end());
    file_stram.close();
    HttpResponse response = HttpResponseBuilder::GetInstance().CreateResponse("{status: ok}", 201);

    return response;
}


//HttpResponse Worker::HttpDelete(HttpRequest* request, const VirtualServer* virtual_server, const VirtualServer::UriProps* location) {
//	std::string file_path = ResolvePagePath(request->GetPath(), virtual_server, location);
//
//	if (IsFileExist(file_path)) {
//		remove(file_path.c_str());
//	}
//	return HttpResponseBuilder::GetInstance().CreateResponse("{\"success\":\"true\"}", 200);
//}
