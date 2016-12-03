class JSONMergeError(TypeError): pass

class BaseInstanceError(JSONMergeError): pass
class HeadInstanceError(JSONMergeError): pass
class SchemaError(JSONMergeError): pass
