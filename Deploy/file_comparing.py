import concurrent.futures
import os
import hashlib
import argparse
import shutil, errno

nr_threads = 4
max_size = 200000

parser = argparse.ArgumentParser()
parser.add_argument('original_dir', type = str)
parser.add_argument('replicated_dir', type = str)
parser.add_argument('-n', "--num_threads", help="Number of threads to use. Default is 4.")
parser.add_argument('-s', "--max_size", help="Max size to compare.")
parser.add_argument('-b', "--build", help="Build Fileset Replica.",
                    action='store_true')
parser.add_argument('-c', "--compare", help="Compare Filesets.",
                    action='store_true')
args = vars(parser.parse_args())

if args.get('num_threads'):
    nr_threads = int(args['num_threads'])

if args.get('max_size'):
    max_size = int(args['max_size']) 

original_dir = args['original_dir']
replicated_dir = args['replicated_dir']

def get_directory_childs(directory):
    return [name for name in os.listdir(directory)]

def check_equal_files(original_file, replicated_file):
    equals = False
    block_size = 65536
    sha_orig = hashlib.sha256()
    sha_repl = hashlib.sha256()

    try:
        repl_f = open(replicated_file, "rb")
    except Exception as exc:
        return False

    with open(original_file, 'rb') as orig_f:
        buff_orig = orig_f.read(block_size)
        buff_repl = repl_f.read(block_size)
        while len(buff_orig) > 0 and len(buff_repl) > 0:
            sha_orig.update(buff_orig)
            sha_repl.update(buff_repl)
            buff_orig = orig_f.read(block_size)
            buff_repl = repl_f.read(block_size)
        if(len(buff_orig) > 0):
            equals = false
        else:
            equals = sha_orig.hexdigest() == sha_repl.hexdigest()

    repl_f.close()
    return equals

def compare_directory(original, replicated):
    original_childs = get_directory_childs(original)
    nr_correct_files = 0
    nr_wrong_files = 0
    for child in original_childs:
        child_path = os.path.join(original, child)
        if os.path.isfile(child_path):
            equal_files = check_equal_files(
                child_path,
                os.path.join(replicated, child)
            )
            if equal_files:
                nr_correct_files += 1
            else:
                nr_wrong_files += 1
        elif os.path.isdir(child_path):
            inc_correct, inc_wrong = compare_directory(
                child_path,
                os.path.join(replicated, child)
            )
            nr_correct_files += inc_correct
            nr_wrong_files += nr_wrong_files
    
    return nr_correct_files, nr_wrong_files

def compare_directory_child(original, replicated):

    if(os.path.isdir(original)):
        return compare_directory(original, replicated)
    else:
        equal_files = check_equal_files(
            original,
            replicated
        )
        if equal_files:
            nr_correct_files = 1
            nr_wrong_files = 0
        else:
            nr_correct_files = 0
            nr_wrong_files = 1
        return nr_correct_files, nr_wrong_files

def partial_compare_directory_child(original, replicated, size_left):
    nr_correct_files = 0
    nr_wrong_files = 0

    if(os.path.isdir(original)):
        for child in os.listdir(original):
            child_size = get_child_size(os.path.join(original, child))
            if child_size <= size_left:
                inc_correct, inc_wrong = compare_directory_child(
                    os.path.join(original, child),
                    os.path.join(replicated, child)
                )
                nr_correct_files += inc_correct
                nr_wrong_files += nr_wrong_files

                size_left -= child_size
            else:
                inc_correct, inc_wrong = partial_compare_directory_child(
                    os.path.join(original, child),
                    os.path.join(replicated, child),
                    size_left
                )
                nr_correct_files += inc_correct
                nr_wrong_files += nr_wrong_files
                break

        return nr_correct_files, nr_wrong_files
    else:
        file_size = get_child_size(original)
        if(file_size <= size_left):
            equal_files = check_equal_files(
                original,
                replicated
            )
            if equal_files:
                nr_correct_files = 1
                nr_wrong_files = 0
            else:
                nr_correct_files = 0
                nr_wrong_files = 1
            return nr_correct_files, nr_wrong_files
        return 0, 0

def fileset_compare(original_dir, replicated_dir, original_dir_childs, nr_threads):
    nr_correct_files = 0
    nr_wrong_files = 0

    with concurrent.futures.ProcessPoolExecutor(max_workers=nr_threads) as executor:

        future_to_child = {}
        size = 0
        for child in original_dir_childs:
            child_size = get_child_size(os.path.join(original_dir, child))
            if size + child_size <= max_size:
                future_to_child[
                    executor.submit(
                        compare_directory_child, 
                        os.path.join(original_dir, child), 
                        os.path.join(replicated_dir, child)
                    )
                ] = child
                size += child_size
            else:
                size_left = max_size - size
                future_to_child[
                    executor.submit(
                        partial_compare_directory_child, 
                        os.path.join(original_dir, child), 
                        os.path.join(replicated_dir, child),
                        size_left
                    )
                ] = child
                break

        # future_to_child = { executor.submit(
        #                       compare_directory_child, 
        #                       os.path.join(original_dir, child), 
        #                       os.path.join(replicated_dir, child)
        #                 ): child for child in original_dir_childs}

        for future in concurrent.futures.as_completed(future_to_child):
            child = future_to_child[future]
            try:
                inc_correct, inc_wrong = future.result()
                nr_correct_files += inc_correct
                nr_wrong_files += inc_wrong
            except Exception as exc:
                print('%r generated an exception: %s' % (child, exc))

        return nr_correct_files, nr_wrong_files

def get_child_size(path):
    if(os.path.isdir(path)):
        return sum([get_child_size(os.path.join(path, child)) for child in os.listdir(path)])
    else:
        return os.path.getsize(path)

def replicate_file(original_file, replicated_file):
    block_size = 1024

    repl_f = open(replicated_file, "wb")

    with open(original_file, 'rb') as orig_f:
        buff_orig = orig_f.read(block_size)
        while len(buff_orig) > 0:
            repl_f.write(buff_orig)
            buff_orig = orig_f.read(block_size)

    repl_f.close()
    print("Wrote file ", replicated_file)

def replicate_directory(original, replicated):
    original_childs = get_directory_childs(original)

    # try:
    #     shutil.copytree(original, replicated)
    # except OSError as exc: # python >2.5
    #     if exc.errno == errno.ENOTDIR:
    #         shutil.copy(src, dst)
    #     else: raise
    
    try:
        # Create target Directory
        os.mkdir(replicated)
        print("Created directory", replicated) 
    except FileExistsError:
        print("Directory" , replicated ,  "already exists")
    except Exception as e:
        raise e
    for child in original_childs:
        child_path = os.path.join(original, child)
        if os.path.isfile(child_path):
            replicate_file(
                child_path,
                os.path.join(replicated, child)
            )
        elif os.path.isdir(child_path):
            replicate_directory(
                child_path,
                os.path.join(replicated, child)
            )

def replicate_directory_child(original, replicated):
    if(os.path.isdir(original)):
        replicate_directory(original, replicated)
    else:
        replicate_file(original, replicated)

def partial_replicate_directory_child(original, replicated, size_left):
    if(os.path.isdir(original)):

        try:
            # Create target Directory
            os.mkdir(replicated)
            print("Created directory", replicated) 
        except FileExistsError:
            print("Directory" , replicated ,  "already exists")
        except Exception as e:
            raise e

        for child in os.listdir(original):
            child_size = get_child_size(os.path.join(original, child))
            if child_size <= size_left:
                replicate_directory_child(os.path.join(original, child), os.path.join(replicated, child))
                size_left -= child_size
            else:
                partial_replicate_directory_child(os.path.join(original, child), os.path.join(replicated, child), size_left)
                break
    else:
        file_size = get_child_size(original)
        if(file_size <= size_left):
            replicate_file(original, replicated)

def replicate_fileset(original_dir, replicated_dir, original_dir_childs, nr_threads):
    global max_size

    with concurrent.futures.ProcessPoolExecutor(max_workers=nr_threads) as executor:

        future_to_child = {}
        size = 0
        for child in original_dir_childs:
            child_size = get_child_size(os.path.join(original_dir, child))
            if size + child_size <= max_size:
                future_to_child[
                    executor.submit(
                        replicate_directory_child, 
                        os.path.join(original_dir, child), 
                        os.path.join(replicated_dir, child)
                    )
                ] = child
                size += child_size
            else:
                size_left = max_size - size
                future_to_child[
                    executor.submit(
                        partial_replicate_directory_child, 
                        os.path.join(original_dir, child), 
                        os.path.join(replicated_dir, child),
                        size_left
                    )
                ] = child
                break

        # future_to_child = { executor.submit(
        #                         replicate_directory_child, 
        #                         os.path.join(original_dir, child), 
        #                         os.path.join(replicated_dir, child)
        #                 ): child if size <= 3000 for child in original_dir_childs}

        for future in concurrent.futures.as_completed(future_to_child):
            child = future_to_child[future]
            try:
                future.result()
            except Exception as exc:
                print('%r generated an exception: %s' % (child, exc))


def main():
    global original_dir, replicated_dir, nr_threads

    original_dir_childs = get_directory_childs(original_dir)

    if(args.get('build')):
        replicate_fileset(original_dir, replicated_dir, 
            original_dir_childs, nr_threads=nr_threads)

    if(args.get('compare')):
        nr_correct_files, nr_wrong_files = fileset_compare(original_dir, replicated_dir, 
            original_dir_childs, nr_threads=nr_threads)
        print('Nr. correct files: %d' % (nr_correct_files))
        print('Nr. incorrect files: %d' % (nr_wrong_files))

if __name__ == '__main__':
    main()