struct Response {
    1: bool status,
    2: optional string value,
}

service LogKeyValue {

    Response Put(1:string key, 2:string value),
	Response Get(1:string key),
    Response Delete(1:string key),
    Response Exist(1:string key)
    
}
