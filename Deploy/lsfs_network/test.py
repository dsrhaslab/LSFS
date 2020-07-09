try:
    import kubernetes
    import openshift
    from openshift.dynamic import DynamicClient
    from openshift.dynamic.exceptions import ResourceNotFoundError, ResourceNotUniqueError
    print "Found it"
except ImportError:
    print "Didnt find it"
