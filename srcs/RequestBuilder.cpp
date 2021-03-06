/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestBuilder.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sergey <sergey@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/05/07 18:20:39 by zytrams           #+#    #+#             */
/*   Updated: 2022/06/21 02:13:49 by sergey           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestBuilder.h"
#include <sstream>

HttpRequestBuilder::HttpRequestBuilder()
	: c_message_line("\r\n")
	, c_message_end("\r\n\r\n")
{}

HttpRequestBuilder& HttpRequestBuilder::GetInstance()
{
	static HttpRequestBuilder m_instance;
	return m_instance;
}

bool HttpRequestBuilder::ParseInitialFields(HttpRequestBuilder::http_request& req, const std::string msg)
{
	size_t	i, j;
	std::string	line;

	// METHOD
	i = msg.find_first_of('\n');
	line = msg.substr(0, i);
	i = line.find_first_of(' ');

	if (i == std::string::npos)
	{
		// std::cerr << "RFL no space after method" << std::endl;
		return false;
	}
	req.m_method.assign(line, 0, i);
	if (ToHttpMethod(req.m_method) == UNKNOWN)
	{
		// std::cerr << "Invalid method requested" << std::endl;
		return false;
	}

	// PATH
	if ((j = line.find_first_not_of(' ', i)) == std::string::npos)
	{
		// std::cerr << "No PATH / HTTP version" << std::endl;
		return false;
	}
	if ((i = line.find_first_of(' ', j)) == std::string::npos)
	{
		std::cerr <<  "No HTTP version" << std::endl;
		return false;
	}
	req.m_path.assign(line, j, i - j);

	// VERSION
	if ((i = line.find_first_not_of(' ', i)) == std::string::npos)
	{
		std::cerr << "No HTTP version" << std::endl;
		return false;
	}
	if (line[i] == 'H' && line[i + 1] == 'T' && line[i + 2] == 'T' && line[i + 3] == 'P' && line[i + 4] == '/')
		req.m_version.assign(line, i + 5, 3);
	if (req.m_version != "1.0" && req.m_version != "1.1")
	{
		std::cerr << "BAD HTTP VERSION (" << req.m_version << ")" << std::endl;
		return false;
	}

	return true;
}

std::string HttpRequestBuilder::GetNext(const std::string& msg, size_t& cur)
{
	std::string		ret;
	size_t			j;

	if (cur == std::string::npos)
		return "";
	j = msg.find_first_of('\n', cur);
	ret = msg.substr(cur, j - cur);
	if (ret[ret.size() - 1] == '\r')
	{
		if (ret.size())
			ret.resize(ret.size() - 1);
	}
	cur = (j == std::string::npos ? j : j + 1);
	return ret;
}

void HttpRequestBuilder::GetQuery(HttpRequestBuilder::http_request& req)
{
	size_t			i;

	i = req.m_path.find_first_of('?');
	if (i != std::string::npos)
	{
		req.m_query.assign(req.m_path, i + 1, std::string::npos);
		req.m_path = req.m_path.substr(0, i);
	}
}

bool HttpRequestBuilder::ParseBoundary(std::string& boundary, const  std::string& line)
{
	if (line.find("boundary=") != std::string::npos)
	{

		size_t	i = line.find_first_of('=');
		i = line.find_first_not_of(' ', i + 1);
		boundary.append(line, i, std::string::npos);
		return true;
	}
	return false;
}

void HttpRequestBuilder::ParseKey(std::string& key, const std::string& line)
{
	std::string	ret;

	size_t	i = line.find_first_of(':');
	key.append(line, 0 , i);
}

void HttpRequestBuilder::ParseValue(std::string& value, const std::string& line)
{
	size_t i;
	std::string	ret;

	i = line.find_first_of(':');
	i = line.find_first_not_of(' ', i + 1);
	if (i != std::string::npos)
		value.append(line, i, std::string::npos);
}

std::string HttpRequestBuilder::MakeHeaderForCGI(std::string& key)
{
	std::transform(key.begin(), key.end(), key.begin(), CGIFormatter);
	return "HTTP_" + key;
}

std::pair<bool, std::string> HttpRequestBuilder::BuildHttpRequestHeader(const std::string& msg, http_request& http_req)
{
	std::string current;
	std::string key;
	std::string value;
	std::string boundary;
	bool is_valid = true;
	size_t cur_size = 0;
	// std::cerr << "Start building request" <<std::endl;

	std::string parsing_msg;
	if (http_req.m_header_size == 0)
	{
		parsing_msg = msg;
		is_valid = ParseInitialFields(http_req, GetNext(parsing_msg, cur_size));
	}
	else
	{
		parsing_msg = msg.substr(http_req.m_header_size);
		is_valid = http_req.m_is_valid;
	}

	while ((current = GetNext(parsing_msg, cur_size)) != "\r" && current != "" && is_valid)
	{
		key = "";
		value = "";

		if (http_req.GetBoundary().empty()
			&& ToHttpMethod(http_req.m_method) == POST
			&& ParseBoundary(boundary, current))
		{
			// std::cerr << "Found tag boundary : " << boundary << std::endl;
			http_req.m_boundary = boundary;
		}
		ParseKey(key, current);
		ParseValue(value, current);
//		if (key.find("Secret") != std::string::npos)
//			http_req.m_cgi_env[MakeHeaderForCGI(key)] = value;
//		else
//		{
			http_req[key] = value;
//		}
	}

	header_iterator itWWWAuth = http_req.find("Www-Authenticate");
	if (itWWWAuth != http_req.end())
	{
		http_req.m_cgi_env["Www-Authenticate"] = itWWWAuth->second;
	}

	// ???????? ?????? Transfer-Encoding , ???? Content-Length ???????????? ????????, ???????? ???????? ?????????????? ??????????????????
	// ???????? ?????? Transfer-Encoding , ???? Content-Length ??????????????????????, ???? ???????? ?????????????? ?????????????? (?????????? ??????, ???????????? ??????????????????????????, ???????????? ???? Content-Length ?????????? ???? ???????? ?? ???????? ????????????)
	// ???????? ???????? Transfer-Encoding: chunked, ???? ???? Content-Length ???????????? ?????????? (?????? ?? ???? ???????????? ?????? ????????). ?????????? ?????? ???????? ?????????????? ???????????????????? ?????????? ???????? '0\r\n\r\n'
	// ?????? ???????????? Transfer-Encoding ???? ?????????? ????????
	header_iterator itTransferEncoding = http_req.find("Transfer-Encoding");
	if (itTransferEncoding == http_req.end())
	{
		header_iterator itConLen = http_req.find("Content-Length");
		if (itConLen == http_req.end())
		{
			http_req.m_body_size = 0;
		}
		else
		{
			http_req.m_body_size = std::stoi(itConLen->second);
		}
	}
	else
	{
		http_req.m_transfer_encoding_status = ToTransferEncoding(itTransferEncoding->second);
	}

	http_req.m_is_valid = is_valid;
	http_req.m_header_size += cur_size;

	// std::cerr << "Read request header with validity status: " + std::to_string(http_req.m_is_valid)  << std::endl;
	return std::make_pair(!boundary.empty(), !boundary.empty() ? "--" + boundary + "--\r\n" : "");
}

void HttpRequestBuilder::BuildHttpRequestBody(HttpRequestBuilder::http_request& http_req, const std::string& msg)
{
	std::string current;

	// std::cerr << "READ BODY" << std::endl;
	if (http_req.m_is_valid)
	{
		size_t end_body = std::string::npos;
		if (!http_req.m_boundary.empty())
		{
			end_body = msg.find("--" + http_req.m_boundary + "--\r\n") - http_req.m_header_size;
		}
		
		current = msg.substr(http_req.m_header_size, end_body);

		std::istringstream iss(current);
		std::string cleared;
		if (http_req.GetTransferEncoding() == CHUNKED)
		{
			bool is_concat = false;
			for (std::string line; std::getline(iss, line); )
			{
				if (is_concat)
				{
                    while(line.size() && line.back() == '\r') {
                        line.pop_back();
                    }
					cleared += line;
				}
				is_concat = !is_concat;
			}
		}
		else
		{
			cleared = current;
		}

		http_req.m_body = std::vector<char>(cleared.begin(), cleared.end());
		// std::cerr << "Read request body with size: " << std::to_string(http_req.m_body.size())  << std::endl;
		GetQuery(http_req);
	}
//	std::cerr << http_req.ToString().substr(0, 10000) << std::endl;
}